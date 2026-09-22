#include "modules/web/web.h"

#include <stdio.h>
#include <string.h>
#include "board_config.h"
#include "modules/console/console.h"
#include "modules/power/power.h"
#include "nx_api.h"
#include "nx_stm32_eth_driver.h"
#include "nxd_dhcp_client.h"
#include "tx_api.h"
#include "web_assets.h"

#define WEB_THREAD_STACK_SIZE 4096U
#define WEB_THREAD_PRIORITY 8U
#define NX_IP_THREAD_STACK_SIZE 2048U
#define NX_IP_THREAD_PRIORITY 2U
#define NX_PACKET_PAYLOAD 1536U
#define NX_PACKET_COUNT 24U
#define NX_ARP_CACHE_SIZE 1024U
#define NX_HTTP_WINDOW 2048U
#define HTTP_REQ_MAX 1536U
#define JSON_MAX 2560U

static TX_THREAD web_thread;
static ULONG web_thread_stack[WEB_THREAD_STACK_SIZE / sizeof(ULONG)];
static NX_PACKET_POOL nx_pool;
static NX_IP nx_ip;
static NX_DHCP nx_dhcp;
static NX_TCP_SOCKET http_socket;
static UCHAR nx_pool_mem[NX_PACKET_COUNT * (NX_PACKET_PAYLOAD + 128U)]
    __attribute__((section(".RxArraySection"), aligned(32)));
static UCHAR nx_ip_stack[NX_IP_THREAD_STACK_SIZE];
static UCHAR nx_arp_cache[NX_ARP_CACHE_SIZE];
static char json_buf[JSON_MAX];

static void print_ip(const char *tag, ULONG addr)
{
    (void)console_print("%s %u.%u.%u.%u\r\n", tag, (unsigned)((addr >> 24) & 0xFFU), (unsigned)((addr >> 16) & 0xFFU),
                        (unsigned)((addr >> 8) & 0xFFU), (unsigned)(addr & 0xFFU));
}

static UINT tcp_send_bytes(NX_TCP_SOCKET *socket, const void *data, ULONG length)
{
    const uint8_t *cursor = (const uint8_t *)data;

    while (length != 0U) {
        NX_PACKET *packet;
        ULONG room;
        ULONG chunk;

        if (nx_packet_allocate(&nx_pool, &packet, NX_TCP_PACKET, NX_WAIT_FOREVER) != NX_SUCCESS)
            return 1;
        room = (ULONG)(packet->nx_packet_data_end - packet->nx_packet_prepend_ptr);
        chunk = length < room ? length : room;
        if (nx_packet_data_append(packet, (VOID *)cursor, chunk, &nx_pool, NX_WAIT_FOREVER) != NX_SUCCESS) {
            nx_packet_release(packet);
            return 1;
        }
        if (nx_tcp_socket_send(socket, packet, NX_WAIT_FOREVER) != NX_SUCCESS) {
            nx_packet_release(packet);
            return 1;
        }
        cursor += chunk;
        length -= chunk;
    }
    return NX_SUCCESS;
}

static UINT http_reply(NX_TCP_SOCKET *socket, const char *status, const char *type, const uint8_t *body, uint32_t body_len)
{
    char hdr[192];
    UINT n = 0U;
    const char *s;
    char lenbuf[12];
    unsigned v = body_len;
    unsigned i = 0U;

    do {
        lenbuf[i++] = (char)('0' + (v % 10U));
        v /= 10U;
    } while (v != 0U);

    for (s = "HTTP/1.0 "; *s != '\0'; ++s)
        hdr[n++] = *s;
    for (s = status; *s != '\0'; ++s)
        hdr[n++] = *s;
    for (s = "\r\nContent-Type: "; *s != '\0'; ++s)
        hdr[n++] = *s;
    for (s = type; *s != '\0'; ++s)
        hdr[n++] = *s;
    for (s = "\r\nContent-Length: "; *s != '\0'; ++s)
        hdr[n++] = *s;
    while (i != 0U)
        hdr[n++] = lenbuf[--i];
    for (s = "\r\nConnection: close\r\n\r\n"; *s != '\0'; ++s)
        hdr[n++] = *s;
    hdr[n] = '\0';

    if (tcp_send_bytes(socket, hdr, n) != NX_SUCCESS)
        return 1;
    if (body_len == 0U)
        return NX_SUCCESS;
    return tcp_send_bytes(socket, body, body_len);
}

static const char *jb(bool v)
{
    return v ? "true" : "false";
}

static int bq_json_fill(char *buf, uint32_t cap)
{
    power_bq_snapshot s;
    int n;
    uint8_t i;

    (void)power_bq_copy_snapshot(&s);
    n = snprintf(buf, cap,
                 "{\"present\":%s,\"part\":%u,\"uptime_ms\":%lu,"
                 "\"adc\":{\"vac_mv\":%lu,\"vbat_mv\":%lu,\"iac_x10\":%d,\"ibat_ma\":%d,\"vfb_mv\":%u,\"ts\":%u},"
                 "\"raw\":{\"s1\":%u,\"s2\":%u,\"s3\":%u,\"fault\":%u},"
                 "\"decoded\":{\"charge\":\"%s\",\"ts\":\"%s\",\"mppt\":\"%s\",\"adc_done\":%s,\"iac_dpm\":%s,"
                 "\"vac_dpm\":%s,\"wd\":%s,\"pg\":%s,\"reverse\":%s,\"cv_timer\":%s,\"vac_uv\":%s,\"vac_ov\":%s,"
                 "\"ibat_ocp\":%s,\"vbat_ov\":%s,\"tshut\":%s,\"safety\":%s,\"drv\":%s},"
                 "\"ctrl\":{\"charge\":%s,\"hiz\":%s,\"reverse\":%s,\"mppt\":%s,\"pfm\":%s,\"ts\":%s,\"jeita\":%s,\"wd\":%u},"
                 "\"cfg\":{\"fb_mv\":%u,\"ichg_ma\":%lu,\"iac_ma\":%lu,\"vac_mv\":%lu,\"ipre_ma\":%lu,\"iterm_ma\":%lu,"
                 "\"en_pre\":%s,\"en_term\":%s},"
                 "\"rev\":{\"vac_mv\":%lu,\"iac_ma\":%lu,\"ibat_rev\":%u,\"uvp_3v3\":%s},"
                 "\"mppt\":{\"perturb\":%u,\"sweep\":%u},"
                 "\"tmr\":{\"safety\":%u,\"safety_en\":%s,\"topoff\":%u,\"vbat_lowv\":%u,\"vrechg\":%u},"
                 "\"gate\":{\"buck_hs\":%u,\"buck_ls\":%u,\"boost_hs\":%u,\"boost_ls\":%u,\"buck_dt\":%u,\"boost_dt\":%u},"
                 "\"pins\":{\"ichg\":%s,\"ilim\":%s,\"ce\":%s,\"stat\":%s,\"pg\":%s},"
                 "\"latch\":{\"fault\":%u,\"f1\":%u,\"f2\":%u,\"ff\":%u},\"log\":[",
                 jb(s.present), s.part, (unsigned long)s.uptime_ms, (unsigned long)s.adc.input_voltage_mv,
                 (unsigned long)s.adc.battery_voltage_mv, (int)s.adc.input_current_ma_x10, (int)s.adc.battery_current_ma,
                 s.adc.feedback_voltage_mv, s.adc.ts_permille, s.raw.status1, s.raw.status2, s.raw.status3, s.raw.fault,
                 bq25756_charge_phase_name(s.decoded.charge), bq25756_ts_state_name(s.decoded.ts),
                 bq25756_mppt_state_name(s.decoded.mppt), jb(s.decoded.adc_done), jb(s.decoded.iac_dpm),
                 jb(s.decoded.vac_dpm), jb(s.decoded.watchdog_expired), jb(s.decoded.pg), jb(s.decoded.reverse),
                 jb(s.decoded.cv_timer), jb(s.decoded.vac_uv), jb(s.decoded.vac_ov), jb(s.decoded.ibat_ocp),
                 jb(s.decoded.vbat_ov), jb(s.decoded.tshut), jb(s.decoded.safety_timer), jb(s.decoded.drv_fault),
                 jb(s.charge_en), jb(s.hiz), jb(s.reverse_en), jb(s.mppt_en), jb(s.pfm), jb(s.ts_en), jb(s.jeita_en),
                 s.watchdog, s.charge.fb_voltage_mv, (unsigned long)s.charge.charge_current_ma,
                 (unsigned long)s.charge.input_current_ma, (unsigned long)s.charge.input_voltage_mv,
                 (unsigned long)s.charge.precharge_current_ma, (unsigned long)s.charge.termination_current_ma,
                 jb(s.charge.enable_precharge), jb(s.charge.enable_termination), (unsigned long)s.reverse.vac_mv,
                 (unsigned long)s.reverse.iac_ma, (unsigned)s.reverse.ibat_rev, jb(s.reverse.uvp_fixed_3v3),
                 (unsigned)s.mppt.perturb, (unsigned)s.mppt.full_sweep, s.safety_timer, jb(s.safety_en), s.topoff,
                 s.vbat_lowv, s.vrechg, (unsigned)s.gate.buck_hs, (unsigned)s.gate.buck_ls, (unsigned)s.gate.boost_hs,
                 (unsigned)s.gate.boost_ls, (unsigned)s.gate.buck_dead_time, (unsigned)s.gate.boost_dead_time,
                 jb(s.pins.ichg_pin_enabled), jb(s.pins.ilim_hiz_pin_enabled), jb(s.pins.ce_pin_enabled),
                 jb(s.pins.stat_pins_enabled), jb(s.pins.pg_pin_enabled), s.latch_fault, s.latch_flag1, s.latch_flag2,
                 s.latch_fault_flag);
    if (n < 0 || (uint32_t)n >= cap)
        return 0;
    for (i = 0U; i < s.log_count; ++i) {
        int m = snprintf(buf + n, cap - (uint32_t)n, "%s{\"t\":%lu,\"kind\":%u,\"bits\":%u}", i == 0U ? "" : ",",
                         (unsigned long)s.log[i].t_ms, s.log[i].kind, s.log[i].bits);
        if (m < 0 || (uint32_t)m >= cap - (uint32_t)n)
            break;
        n += m;
    }
    if ((uint32_t)n + 3U < cap) {
        buf[n++] = ']';
        buf[n++] = '}';
        buf[n] = '\0';
    }
    return n;
}

static int shp_json_fill(char *buf, uint32_t cap)
{
    power_shp_snapshot s;
    int n;
    uint8_t i;

    (void)power_shp_copy_snapshot(&s);
    n = snprintf(
        buf, cap,
        "{\"present\":%s,\"option0\":%u,\"uptime_ms\":%lu,"
        "\"adc\":{\"vbus_mv\":%lu,\"vbat_mv\":%lu,\"ibus_ma\":%d,\"iotg_ma\":%d,\"ibat_ma\":%d,\"ts\":%u},"
        "\"raw\":{\"s0\":%u,\"s1\":%u,\"s2\":%u},"
        "\"decoded\":{\"charge\":\"%s\",\"ts\":\"%s\",\"mppt\":\"%s\",\"ibus_reg\":%s,\"vindpm\":%s,\"pg\":%s,"
        "\"vbus\":%s,\"vbus_ov\":%s,\"vbat_ov\":%s,\"il_clamp\":%s,\"reverse\":%s,\"sync\":%s,\"otg_ov\":%s,"
        "\"otg_uv\":%s,\"safety\":%s},"
        "\"ctrl\":{\"charge\":%s,\"hiz\":%s,\"reverse\":%s,\"mppt\":%s,\"pfm\":%s,\"ts\":%s,\"jeita\":%s},"
        "\"cfg\":{\"fb_mv\":%u,\"ichg_ma\":%lu,\"iac_ma\":%lu,\"vindpm_mv\":%u,\"ipre_ma\":%lu,\"iterm_ma\":%lu,"
        "\"en_pre\":%s,\"en_term\":%s,\"en_float\":%s},"
        "\"rev\":{\"vbus_mv\":%lu,\"ibus_ma\":%lu,\"fb_pin\":%s},"
        "\"mppt\":{\"perturb\":%u,\"sweep\":%u,\"step\":%u},"
        "\"tmr\":{\"safety\":%u,\"safety_en\":%s,\"pre\":%u,\"pre_en\":%s,\"vbat_lowv\":%u,\"vrechg\":%u,\"vfloat\":%u},"
        "\"pins\":{\"ibus\":%s,\"ibat\":%s,\"otg_fb\":%s},"
        "\"latch\":{\"s0\":%u,\"s1\":%u,\"s2\":%u},\"log\":[",
        jb(s.present), s.option0, (unsigned long)s.uptime_ms, (unsigned long)s.adc.input_voltage_mv,
        (unsigned long)s.adc.battery_voltage_mv, (int)s.adc.input_current_ma, (int)s.adc.otg_current_ma,
        (int)s.adc.battery_current_ma, s.adc.ts_permille, s.raw.status0, s.raw.status1, s.raw.status2,
        shp8808_charge_phase_name(s.decoded.charge), shp8808_ts_state_name(s.decoded.ts),
        shp8808_mppt_state_name(s.decoded.mppt), jb(s.decoded.ibus_reg), jb(s.decoded.vindpm), jb(s.decoded.pg),
        jb(s.decoded.vbus_present), jb(s.decoded.vbus_ov), jb(s.decoded.vbat_ov), jb(s.decoded.il_clamp),
        jb(s.decoded.reverse), jb(s.decoded.sync), jb(s.decoded.otg_ov), jb(s.decoded.otg_uv),
        jb(s.decoded.safety_timer), jb(s.charge_en), jb(s.hiz), jb(s.reverse_en), jb(s.mppt_en), jb(s.pfm), jb(s.ts_en),
        jb(s.jeita_en), s.charge.fb_voltage_mv, (unsigned long)s.charge.charge_current_ma,
        (unsigned long)s.charge.input_current_ma, s.charge.vindpm_ref_mv, (unsigned long)s.charge.precharge_current_ma,
        (unsigned long)s.charge.termination_current_ma, jb(s.charge.enable_precharge), jb(s.charge.enable_termination),
        jb(s.charge.enable_float), (unsigned long)s.reverse.vbus_mv, (unsigned long)s.reverse.ibus_ma,
        jb(s.reverse.fb_pin_enabled), (unsigned)s.mppt.perturb, (unsigned)s.mppt.full_sweep, (unsigned)s.mppt.step,
        s.safety_timer, jb(s.safety_en), s.precharge_timer, jb(s.precharge_timer_en), s.vbat_lowv, s.vrechg, s.vfloat,
        jb(s.pins.ibus_pin_enabled), jb(s.pins.ibat_pin_enabled), jb(s.pins.otg_fb_pin_enabled), s.latch0, s.latch1,
        s.latch2);
    if (n < 0 || (uint32_t)n >= cap)
        return 0;
    for (i = 0U; i < s.log_count; ++i) {
        int m = snprintf(buf + n, cap - (uint32_t)n, "%s{\"t\":%lu,\"kind\":%u,\"bits\":%u}", i == 0U ? "" : ",",
                         (unsigned long)s.log[i].t_ms, s.log[i].kind, s.log[i].bits);
        if (m < 0 || (uint32_t)m >= cap - (uint32_t)n)
            break;
        n += m;
    }
    if ((uint32_t)n + 3U < cap) {
        buf[n++] = ']';
        buf[n++] = '}';
        buf[n] = '\0';
    }
    return n;
}

static int path_is(const char *path, const char *want)
{
    while (*want != '\0') {
        if (*path++ != *want++)
            return 0;
    }
    return *path == '\0' || *path == ' ' || *path == '?' || *path == '\r';
}

static uint32_t parse_content_length(const char *req, uint32_t n)
{
    const char *p;
    uint32_t i;
    uint32_t v = 0U;

    for (i = 0U; i + 16U < n; ++i) {
        if ((req[i] == 'C' || req[i] == 'c') && memcmp(req + i + 1, "ontent-Length:", 14) == 0) {
            p = req + i + 15;
            while (p < req + n && (*p == ' ' || *p == '\t'))
                ++p;
            while (p < req + n && *p >= '0' && *p <= '9')
                v = v * 10U + (uint32_t)(*p++ - '0');
            return v;
        }
    }
    return 0U;
}

static const char *hdr_end(const char *req, uint32_t n, uint32_t *hlen)
{
    uint32_t i;
    for (i = 0U; i + 3U < n; ++i) {
        if (req[i] == '\r' && req[i + 1] == '\n' && req[i + 2] == '\r' && req[i + 3] == '\n') {
            *hlen = i + 4U;
            return req + *hlen;
        }
    }
    *hlen = 0U;
    return 0;
}

static void http_route(NX_TCP_SOCKET *socket, const char *req, uint32_t req_len)
{
    const char *path;
    const char *body;
    uint32_t hlen = 0U;
    uint32_t blen;
    int json_len;
    static const uint8_t ok[] = "{\"ok\":true}";
    static const uint8_t bad[] = "{\"ok\":false}";

    if (req_len < 5U)
        return;
    path = req;
    while (*path != ' ' && *path != '\0')
        ++path;
    if (*path == ' ')
        ++path;

    body = hdr_end(req, req_len, &hlen);
    blen = 0U;
    if (body != 0) {
        uint32_t cl = parse_content_length(req, hlen);
        blen = req_len - hlen;
        if (cl > 0U && cl < blen)
            blen = cl;
    }

    if (req[0] == 'G') {
        if (path_is(path, "/") || path_is(path, "/index.html"))
            (void)http_reply(socket, "200 OK", "text/html; charset=utf-8", web_index_html, web_index_html_len);
        else if (path_is(path, "/style.css"))
            (void)http_reply(socket, "200 OK", "text/css; charset=utf-8", web_style_css, web_style_css_len);
        else if (path_is(path, "/app.js"))
            (void)http_reply(socket, "200 OK", "application/javascript; charset=utf-8", web_app_js, web_app_js_len);
        else if (path_is(path, "/api/bq25756")) {
            json_len = bq_json_fill(json_buf, sizeof(json_buf));
            (void)http_reply(socket, "200 OK", "application/json", (const uint8_t *)json_buf, (uint32_t)json_len);
        } else if (path_is(path, "/api/shp8808")) {
            json_len = shp_json_fill(json_buf, sizeof(json_buf));
            (void)http_reply(socket, "200 OK", "application/json", (const uint8_t *)json_buf, (uint32_t)json_len);
        } else {
            (void)http_reply(socket, "404 Not Found", "text/plain; charset=utf-8", (const uint8_t *)"not found", 9U);
        }
        return;
    }

    if (req[0] == 'P' && path_is(path, "/api/bq25756")) {
        if (body != 0 && power_bq_command(body, blen) == 0)
            (void)http_reply(socket, "200 OK", "application/json", ok, sizeof(ok) - 1U);
        else
            (void)http_reply(socket, "400 Bad Request", "application/json", bad, sizeof(bad) - 1U);
        return;
    }
    if (req[0] == 'P' && path_is(path, "/api/shp8808")) {
        if (body != 0 && power_shp_command(body, blen) == 0)
            (void)http_reply(socket, "200 OK", "application/json", ok, sizeof(ok) - 1U);
        else
            (void)http_reply(socket, "400 Bad Request", "application/json", bad, sizeof(bad) - 1U);
        return;
    }
    (void)http_reply(socket, "404 Not Found", "text/plain; charset=utf-8", (const uint8_t *)"not found", 9U);
}

static void http_serve_one(void)
{
    char req[HTTP_REQ_MAX];
    uint32_t total = 0U;
    uint32_t hlen = 0U;
    uint32_t need = 0U;
    UINT waits = 0U;

    if (nx_tcp_server_socket_accept(&http_socket, NX_WAIT_FOREVER) != NX_SUCCESS)
        return;

    while (total < HTTP_REQ_MAX - 1U && waits < 8U) {
        NX_PACKET *packet;
        ULONG copied = 0U;
        UINT timeout = total == 0U ? 2000U : 500U;

        if (nx_tcp_socket_receive(&http_socket, &packet, timeout) != NX_SUCCESS) {
            ++waits;
            if (hdr_end(req, total, &hlen) != 0)
                break;
            continue;
        }
        (void)nx_packet_data_extract_offset(packet, 0, req + total, HTTP_REQ_MAX - 1U - total, &copied);
        nx_packet_release(packet);
        total += (uint32_t)copied;
        req[total] = '\0';
        if (hdr_end(req, total, &hlen) != 0) {
            need = parse_content_length(req, hlen);
            if (total >= hlen + need)
                break;
        }
    }

    if (total > 0U)
        http_route(&http_socket, req, total);

    (void)nx_tcp_socket_disconnect(&http_socket, 1000);
    (void)nx_tcp_server_socket_unaccept(&http_socket);
    (void)nx_tcp_server_socket_relisten(&nx_ip, 80, &http_socket);
}

static UINT net_start(void)
{
    ULONG actual;
    ULONG ip_addr;
    ULONG mask;

    (void)console_puts("net: packet pool + ip instance");
    nx_system_initialize();
    if (nx_packet_pool_create(&nx_pool, "nx pool", NX_PACKET_PAYLOAD, nx_pool_mem, sizeof(nx_pool_mem)) != NX_SUCCESS)
        return 1;
    if (nx_ip_create(&nx_ip, "nx ip", 0, 0, &nx_pool, nx_stm32_eth_driver, nx_ip_stack, sizeof(nx_ip_stack),
                     NX_IP_THREAD_PRIORITY) != NX_SUCCESS)
        return 1;
    if (nx_arp_enable(&nx_ip, nx_arp_cache, sizeof(nx_arp_cache)) != NX_SUCCESS || nx_icmp_enable(&nx_ip) != NX_SUCCESS ||
        nx_tcp_enable(&nx_ip) != NX_SUCCESS || nx_udp_enable(&nx_ip) != NX_SUCCESS)
        return 1;

    (void)console_puts("net: dhcp start");
    if (nx_dhcp_create(&nx_dhcp, &nx_ip, "dhcp") != NX_SUCCESS || nx_dhcp_start(&nx_dhcp) != NX_SUCCESS)
        return 1;

    if (nx_ip_status_check(&nx_ip, NX_IP_ADDRESS_RESOLVED, &actual, BOARD_NET_DHCP_WAIT_MS) != NX_SUCCESS) {
        (void)console_puts("net: dhcp timeout, using static 192.168.1.80");
        (void)nx_dhcp_stop(&nx_dhcp);
        (void)nx_dhcp_delete(&nx_dhcp);
        if (nx_ip_address_set(&nx_ip, BOARD_NET_STATIC_IP, BOARD_NET_STATIC_MASK) != NX_SUCCESS)
            return 1;
        (void)nx_ip_gateway_address_set(&nx_ip, BOARD_NET_STATIC_GW);
    }

    (void)nx_ip_address_get(&nx_ip, &ip_addr, &mask);
    print_ip("net ip", ip_addr);
    print_ip("net mask", mask);
    if (nx_ip_interface_status_check(&nx_ip, 0, NX_IP_LINK_ENABLED, &actual, 1) == NX_SUCCESS)
        (void)console_puts("net: link up");
    else
        (void)console_puts("net: link down (plug cable, static IP still set)");
    return NX_SUCCESS;
}

static void web_thread_entry(ULONG arg)
{
    (void)arg;
    (void)console_puts("net: start");
    if (net_start() != NX_SUCCESS) {
        (void)console_puts("net: start failed");
        return;
    }
    if (nx_tcp_socket_create(&nx_ip, &http_socket, "http", NX_IP_NORMAL, NX_FRAGMENT_OKAY, NX_IP_TIME_TO_LIVE,
                             NX_HTTP_WINDOW, NX_NULL, NX_NULL) != NX_SUCCESS ||
        nx_tcp_server_socket_listen(&nx_ip, 80, &http_socket, 3, NX_NULL) != NX_SUCCESS) {
        (void)console_puts("http: listen failed");
        return;
    }
    (void)console_puts("http: listen :80 power console");
    for (;;)
        http_serve_one();
}

int web_module_start(void)
{
    if (tx_thread_create(&web_thread, "web", web_thread_entry, 0, web_thread_stack, sizeof(web_thread_stack),
                         WEB_THREAD_PRIORITY, WEB_THREAD_PRIORITY, TX_NO_TIME_SLICE, TX_AUTO_START) != TX_SUCCESS)
        return -1;
    return 0;
}
