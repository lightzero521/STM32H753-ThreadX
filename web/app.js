const S1 = ["CHG0", "CHG1", "CHG2", "WD", "b4", "VAC_DPM", "IAC_DPM", "ADC_DONE"];
const S2 = ["MPPT0", "MPPT1", "b2", "b3", "TS0", "TS1", "TS2", "PG"];
const S3 = ["b0", "b1", "REVERSE", "CV_TMR", "b4", "b5", "b6", "b7"];
const FAULT = ["b0", "DRV", "SAFETY", "TSHUT", "VBAT_OV", "IBAT_OCP", "VAC_OV", "VAC_UV"];
const F1 = ["CHG", "b1", "b2", "WD", "b4", "VAC_DPM", "IAC_DPM", "ADC_DONE"];
const F2 = ["MPPT", "b1", "b2", "TS", "b4", "b5", "b6", "PG"];
const FF = FAULT;
const KIND = ["FAULT 0x24", "FLAG1 0x25", "FLAG2 0x26", "FAULT_FLAG 0x27"];
const KIND_BITS = [FAULT, F1, F2, FF];
const S0_SHP = ["b0", "IL_CLP", "VBATOV", "VBUSOV", "VBUS", "PG", "VINDPM", "IBUSREG"];
const S1_SHP = ["OTG_UVP", "OTG_OVP", "TRICKLE_TMR", "PRECHG_TMR", "FAST_TMR", "CHG0", "CHG1", "CHG2"];
const S2_SHP = ["SYNC", "AUTO_REV", "MPPT0", "MPPT1", "TS_HOT", "TS_WARM", "TS_COOL", "TS_COLD"];
const KIND_SHP = ["STATUS0 0x20", "STATUS1 0x21", "STATUS2 0x22"];
const KIND_BITS_SHP = [S0_SHP, S1_SHP, S2_SHP];

const hist = { t: [], vac: [], vbat: [], iac: [], ibat: [] };
const shpHist = { t: [], vac: [], vbat: [], iac: [], ibat: [] };
const MAX_PT = 60;
let chart;
let shpChart;
let skipFill = false;
let chip = "shp";

function bits(val, names) {
  const out = [];
  for (let i = 0; i < 8; i++) {
    if (val & (1 << i)) out.push(names[i] || ("b" + i));
  }
  return out.length ? out.join(" ") : "—";
}

function hex2(v) {
  return "0x" + (v & 0xff).toString(16).padStart(2, "0");
}

function fillSelect(sel, items) {
  sel.innerHTML = items.map((t, i) => `<option value="${i}">${t}</option>`).join("");
}

function busyInput() {
  const el = document.activeElement;
  return el && (el.tagName === "INPUT" || el.tagName === "SELECT" || el.tagName === "BUTTON");
}

function setVal(id, v, isCheck) {
  const el = document.getElementById(id);
  if (!el || document.activeElement === el) return;
  if (isCheck) el.checked = !!v;
  else el.value = String(v);
}

async function api(body, path) {
  const opt = body
    ? { method: "POST", headers: { "Content-Type": "application/json" }, body: JSON.stringify(body) }
    : { method: "GET" };
  const r = await fetch(path || (chip === "shp" ? "/api/shp8808" : "/api/bq25756"), opt);
  return r.json();
}

function pushHist(s, h, chartObj, names) {
  const t = (s.uptime_ms / 1000).toFixed(1);
  h.t.push(t);
  h.vac.push((s.adc.vac_mv != null ? s.adc.vac_mv : s.adc.vbus_mv) / 1000);
  h.vbat.push((s.adc.vbat_mv) / 1000);
  h.iac.push(s.adc.iac_x10 != null ? s.adc.iac_x10 / 10000 : s.adc.ibus_ma / 1000);
  h.ibat.push(s.adc.ibat_ma / 1000);
  if (h.t.length > MAX_PT) {
    h.t.shift();
    h.vac.shift();
    h.vbat.shift();
    h.iac.shift();
    h.ibat.shift();
  }
  if (!chartObj) return;
  chartObj.setOption({
    animation: false,
    grid: { left: 48, right: 48, top: 24, bottom: 28 },
    legend: { data: names },
    xAxis: { type: "category", data: h.t, boundaryGap: false },
    yAxis: [
      { type: "value", name: "V", min: 0 },
      { type: "value", name: "A" }
    ],
    series: [
      { name: names[0], type: "line", showSymbol: false, data: h.vac },
      { name: names[1], type: "line", showSymbol: false, data: h.vbat },
      { name: names[2], type: "line", yAxisIndex: 1, showSymbol: false, data: h.iac },
      { name: names[3], type: "line", yAxisIndex: 1, showSymbol: false, data: h.ibat }
    ]
  });
}

function renderRegs(s) {
  const r = s.raw;
  const d = s.decoded;
  document.getElementById("regs").innerHTML =
    `<p class="bits">STATUS1 ${hex2(r.s1)} ${bits(r.s1, S1)} · ${d.charge}` +
    ` IAC_DPM ${d.iac_dpm | 0} VAC_DPM ${d.vac_dpm | 0} WD ${d.wd | 0} ADC ${d.adc_done | 0}</p>` +
    `<p class="bits">STATUS2 ${hex2(r.s2)} ${bits(r.s2, S2)} · PG ${d.pg | 0} TS ${d.ts} MPPT ${d.mppt}</p>` +
    `<p class="bits">STATUS3 ${hex2(r.s3)} ${bits(r.s3, S3)} · REV ${d.reverse | 0} CV_TMR ${d.cv_timer | 0}</p>` +
    `<p class="bits">FAULT ${hex2(r.fault)} ${bits(r.fault, FAULT)}` +
    ` UV ${d.vac_uv | 0} OV ${d.vac_ov | 0} IBAT_OCP ${d.ibat_ocp | 0}` +
    ` VBAT_OV ${d.vbat_ov | 0} TSHUT ${d.tshut | 0} SAFETY ${d.safety | 0} DRV ${d.drv | 0}</p>`;
}

function renderLatch(s) {
  const l = s.latch;
  document.getElementById("latch").textContent =
    `FAULT ${hex2(l.fault)} ${bits(l.fault, FAULT)} | FLAG1 ${hex2(l.f1)} ${bits(l.f1, F1)} | ` +
    `FLAG2 ${hex2(l.f2)} ${bits(l.f2, F2)} | FAULT_FLAG ${hex2(l.ff)} ${bits(l.ff, FF)}`;
  const tb = document.getElementById("log");
  tb.innerHTML = (s.log || []).slice().reverse().map((e) => {
    const names = KIND_BITS[e.kind] || FAULT;
    return `<tr><td>${e.t}</td><td>${KIND[e.kind] || e.kind}</td><td>${hex2(e.bits)}</td><td>${bits(e.bits, names)}</td></tr>`;
  }).join("");
}

function fillForm(s) {
  if (busyInput() || skipFill) return;
  const c = s.cfg;
  setVal("fb_mv", c.fb_mv);
  setVal("ichg_ma", c.ichg_ma);
  setVal("iac_ma", c.iac_ma);
  setVal("vac_mv", c.vac_mv);
  setVal("ipre_ma", c.ipre_ma);
  setVal("iterm_ma", c.iterm_ma);
  setVal("en_pre", c.en_pre, true);
  setVal("en_term", c.en_term, true);
  setVal("hiz", s.ctrl.hiz ? 1 : 0);
  setVal("reverse", s.ctrl.reverse ? 1 : 0);
  setVal("mppt", s.ctrl.mppt ? 1 : 0);
  setVal("pfm", s.ctrl.pfm ? 1 : 0);
  setVal("ts", s.ctrl.ts ? 1 : 0);
  setVal("jeita", s.ctrl.jeita ? 1 : 0);
  setVal("rev_vac_mv", s.rev.vac_mv);
  setVal("rev_iac_ma", s.rev.iac_ma);
  setVal("ibat_rev", s.rev.ibat_rev);
  setVal("uvp_3v3", s.rev.uvp_3v3, true);
  setVal("perturb", s.mppt.perturb);
  setVal("sweep", s.mppt.sweep);
  setVal("watchdog", s.ctrl.wd);
  setVal("safety_timer", s.tmr.safety);
  setVal("safety_en", s.tmr.safety_en, true);
  setVal("topoff", s.tmr.topoff);
  setVal("vbat_lowv", s.tmr.vbat_lowv);
  setVal("vrechg", s.tmr.vrechg);
  setVal("buck_hs", s.gate.buck_hs);
  setVal("buck_ls", s.gate.buck_ls);
  setVal("boost_hs", s.gate.boost_hs);
  setVal("boost_ls", s.gate.boost_ls);
  setVal("buck_dt", s.gate.buck_dt);
  setVal("boost_dt", s.gate.boost_dt);
  setVal("pin_ichg", s.pins.ichg, true);
  setVal("pin_ilim", s.pins.ilim, true);
  setVal("pin_ce", s.pins.ce, true);
  setVal("pin_stat", s.pins.stat, true);
  setVal("pin_pg", s.pins.pg, true);
}

function payload(cmd) {
  const n = (id) => Number(document.getElementById(id).value);
  const c = (id) => document.getElementById(id).checked;
  const s = (id) => document.getElementById(id).value === "1";
  if (cmd === "hiz" || cmd === "reverse" || cmd === "mppt" || cmd === "pfm" || cmd === "ts" || cmd === "jeita")
    return { cmd, on: s(cmd) };
  if (cmd === "charge_cfg")
    return {
      cmd, fb_mv: n("fb_mv"), ichg_ma: n("ichg_ma"), iac_ma: n("iac_ma"), vac_mv: n("vac_mv"),
      ipre_ma: n("ipre_ma"), iterm_ma: n("iterm_ma"), en_pre: c("en_pre"), en_term: c("en_term")
    };
  if (cmd === "reverse_cfg")
    return { cmd, vac_mv: n("rev_vac_mv"), iac_ma: n("rev_iac_ma"), ibat_rev: n("ibat_rev"), uvp_3v3: c("uvp_3v3") };
  if (cmd === "mppt_cfg")
    return { cmd, perturb: n("perturb"), sweep: n("sweep") };
  if (cmd === "watchdog")
    return { cmd, period: n("watchdog") };
  if (cmd === "safety")
    return { cmd, timer: n("safety_timer"), on: c("safety_en") };
  if (cmd === "topoff")
    return { cmd, timer: n("topoff") };
  if (cmd === "vbat_lowv")
    return { cmd, v: n("vbat_lowv") };
  if (cmd === "vrechg")
    return { cmd, v: n("vrechg") };
  if (cmd === "gate")
    return {
      cmd, buck_hs: n("buck_hs"), buck_ls: n("buck_ls"), boost_hs: n("boost_hs"), boost_ls: n("boost_ls"),
      buck_dt: n("buck_dt"), boost_dt: n("boost_dt")
    };
  if (cmd === "pins")
    return {
      cmd, ichg: c("pin_ichg"), ilim: c("pin_ilim"), ce: c("pin_ce"), stat: c("pin_stat"), pg: c("pin_pg")
    };
  return { cmd };
}

async function tickBq() {
  try {
    const s = await api(null, "/api/bq25756");
    const conn = document.getElementById("conn");
    if (!s.present) {
      conn.textContent = "BQ25756 未连接";
      conn.className = "bad";
      return;
    }
    conn.textContent = "part 0x" + s.part.toString(16) + " 充电 " + (s.ctrl.charge ? "开" : "关");
    conn.className = s.raw.fault || s.latch.fault ? "bad" : "ok";
    document.getElementById("meta").textContent =
      `VAC ${s.adc.vac_mv} mV  VBAT ${s.adc.vbat_mv} mV  IAC ${(s.adc.iac_x10 / 10).toFixed(1)} mA  IBAT ${s.adc.ibat_ma} mA  VFB ${s.adc.vfb_mv} mV`;
    renderRegs(s);
    renderLatch(s);
    fillForm(s);
    pushHist(s, hist, chart, ["VAC", "VBAT", "IAC", "IBAT"]);
  } catch (e) {
    document.getElementById("conn").textContent = "HTTP 失败";
    document.getElementById("conn").className = "bad";
  }
}

function renderShpRegs(s) {
  const r = s.raw;
  const d = s.decoded;
  document.getElementById("shp-regs").innerHTML =
    `<p class="bits">STATUS0 ${hex2(r.s0)} ${bits(r.s0, S0_SHP)} · PG ${d.pg | 0} VBUS ${d.vbus | 0}` +
    ` VINDPM ${d.vindpm | 0} IBUSREG ${d.ibus_reg | 0} OV ${d.vbus_ov | 0}/${d.vbat_ov | 0}</p>` +
    `<p class="bits">STATUS1 ${hex2(r.s1)} ${bits(r.s1, S1_SHP)} · ${d.charge}` +
    ` OTG_OV ${d.otg_ov | 0} OTG_UV ${d.otg_uv | 0} SAFETY ${d.safety | 0}</p>` +
    `<p class="bits">STATUS2 ${hex2(r.s2)} ${bits(r.s2, S2_SHP)} · TS ${d.ts} MPPT ${d.mppt} REV ${d.reverse | 0}</p>`;
}

function renderShpLatch(s) {
  const l = s.latch;
  document.getElementById("shp-latch").textContent =
    `S0 ${hex2(l.s0)} ${bits(l.s0, S0_SHP)} | S1 ${hex2(l.s1)} ${bits(l.s1, S1_SHP)} | S2 ${hex2(l.s2)} ${bits(l.s2, S2_SHP)}`;
  const tb = document.getElementById("shp-log");
  tb.innerHTML = (s.log || []).slice().reverse().map((e) => {
    const names = KIND_BITS_SHP[e.kind] || S0_SHP;
    return `<tr><td>${e.t}</td><td>${KIND_SHP[e.kind] || e.kind}</td><td>${hex2(e.bits)}</td><td>${bits(e.bits, names)}</td></tr>`;
  }).join("");
}

function fillShpForm(s) {
  if (busyInput() || skipFill) return;
  const c = s.cfg;
  setVal("shp_fb_mv", c.fb_mv);
  setVal("shp_ichg_ma", c.ichg_ma);
  setVal("shp_iac_ma", c.iac_ma);
  setVal("shp_vindpm_mv", c.vindpm_mv);
  setVal("shp_ipre_ma", c.ipre_ma);
  setVal("shp_iterm_ma", c.iterm_ma);
  setVal("shp_en_pre", c.en_pre, true);
  setVal("shp_en_term", c.en_term, true);
  setVal("shp_en_float", c.en_float, true);
  setVal("shp_hiz", s.ctrl.hiz ? 1 : 0);
  setVal("shp_reverse", s.ctrl.reverse ? 1 : 0);
  setVal("shp_mppt", s.ctrl.mppt ? 1 : 0);
  setVal("shp_pfm", s.ctrl.pfm ? 1 : 0);
  setVal("shp_ts", s.ctrl.ts ? 1 : 0);
  setVal("shp_jeita", s.ctrl.jeita ? 1 : 0);
  setVal("shp_rev_vbus_mv", s.rev.vbus_mv);
  setVal("shp_rev_ibus_ma", s.rev.ibus_ma);
  setVal("shp_rev_fb", s.rev.fb_pin, true);
  setVal("shp_perturb", s.mppt.perturb);
  setVal("shp_sweep", s.mppt.sweep);
  setVal("shp_step", s.mppt.step);
  setVal("shp_safety_timer", s.tmr.safety);
  setVal("shp_safety_en", s.tmr.safety_en, true);
  setVal("shp_pre_timer", s.tmr.pre);
  setVal("shp_pre_en", s.tmr.pre_en, true);
  setVal("shp_vbat_lowv", s.tmr.vbat_lowv);
  setVal("shp_vrechg", s.tmr.vrechg);
  setVal("shp_vfloat", s.tmr.vfloat);
  setVal("shp_pin_ibus", s.pins.ibus, true);
  setVal("shp_pin_ibat", s.pins.ibat, true);
  setVal("shp_pin_otg_fb", s.pins.otg_fb, true);
}

function shpPayload(cmd) {
  const n = (id) => Number(document.getElementById(id).value);
  const c = (id) => document.getElementById(id).checked;
  const s = (id) => document.getElementById(id).value === "1";
  if (cmd === "hiz" || cmd === "reverse" || cmd === "mppt" || cmd === "pfm" || cmd === "ts" || cmd === "jeita")
    return { cmd, on: s("shp_" + cmd) };
  if (cmd === "charge_cfg")
    return {
      cmd, fb_mv: n("shp_fb_mv"), ichg_ma: n("shp_ichg_ma"), iac_ma: n("shp_iac_ma"),
      vindpm_mv: n("shp_vindpm_mv"), ipre_ma: n("shp_ipre_ma"), iterm_ma: n("shp_iterm_ma"),
      en_pre: c("shp_en_pre"), en_term: c("shp_en_term"), en_float: c("shp_en_float")
    };
  if (cmd === "reverse_cfg")
    return { cmd, vbus_mv: n("shp_rev_vbus_mv"), ibus_ma: n("shp_rev_ibus_ma"), fb_pin: c("shp_rev_fb") };
  if (cmd === "mppt_cfg")
    return { cmd, perturb: n("shp_perturb"), sweep: n("shp_sweep"), step: n("shp_step") };
  if (cmd === "safety")
    return { cmd, timer: n("shp_safety_timer"), on: c("shp_safety_en") };
  if (cmd === "precharge_tmr")
    return { cmd, timer: n("shp_pre_timer"), on: c("shp_pre_en") };
  if (cmd === "vbat_lowv")
    return { cmd, v: n("shp_vbat_lowv") };
  if (cmd === "vrechg")
    return { cmd, v: n("shp_vrechg") };
  if (cmd === "vfloat")
    return { cmd, v: n("shp_vfloat") };
  if (cmd === "pins")
    return { cmd, ibus: c("shp_pin_ibus"), ibat: c("shp_pin_ibat"), otg_fb: c("shp_pin_otg_fb") };
  return { cmd };
}

async function tickShp() {
  try {
    const s = await api(null, "/api/shp8808");
    const conn = document.getElementById("conn");
    if (!s.present) {
      conn.textContent = "SHP8808 未连接";
      conn.className = "bad";
      return;
    }
    conn.textContent = "option0 0x" + s.option0.toString(16) + " 充电 " + (s.ctrl.charge ? "开" : "关");
    conn.className = (s.raw.s0 & 0x0e) || s.latch.s0 & 0x0e ? "bad" : "ok";
    document.getElementById("shp-meta").textContent =
      `VBUS ${s.adc.vbus_mv} mV  VBAT ${s.adc.vbat_mv} mV  IBUS ${s.adc.ibus_ma} mA  IBAT ${s.adc.ibat_ma} mA  IOTG ${s.adc.iotg_ma} mA`;
    renderShpRegs(s);
    renderShpLatch(s);
    fillShpForm(s);
    pushHist(s, shpHist, shpChart, ["VBUS", "VBAT", "IBUS", "IBAT"]);
  } catch (e) {
    document.getElementById("conn").textContent = "HTTP 失败";
    document.getElementById("conn").className = "bad";
  }
}

function switchChip(name) {
  chip = name;
  document.getElementById("tab-shp").classList.toggle("on", name === "shp");
  document.getElementById("tab-bq").classList.toggle("on", name === "bq");
  document.getElementById("panel-shp").classList.toggle("on", name === "shp");
  document.getElementById("panel-bq").classList.toggle("on", name === "bq");
  if (name === "shp" && shpChart) shpChart.resize();
  if (name === "bq" && chart) chart.resize();
  tick();
}

async function tick() {
  if (chip === "shp")
    return tickShp();
  return tickBq();
}

function bind() {
  document.querySelectorAll(".drv").forEach((el) => fillSelect(el, ["最快", "较快", "较慢", "最慢"]));
  document.querySelectorAll(".dt").forEach((el) => fillSelect(el, ["45ns", "75ns", "105ns", "135ns"]));
  if (window.echarts) {
    chart = echarts.init(document.getElementById("chart"));
    shpChart = echarts.init(document.getElementById("shp-chart"));
  }
  document.getElementById("tab-shp").onclick = () => switchChip("shp");
  document.getElementById("tab-bq").onclick = () => switchChip("bq");
  document.getElementById("btn-chg-on").onclick = () => api({ cmd: "charge", on: true }, "/api/bq25756");
  document.getElementById("btn-chg-off").onclick = () => api({ cmd: "charge", on: false }, "/api/bq25756");
  document.getElementById("btn-kick").onclick = () => api({ cmd: "kick" }, "/api/bq25756");
  document.getElementById("btn-reset").onclick = () => api({ cmd: "reset" }, "/api/bq25756");
  document.getElementById("btn-clear").onclick = () => api({ cmd: "clear_latch" }, "/api/bq25756");
  document.getElementById("btn-sweep").onclick = () => api({ cmd: "mppt_sweep" }, "/api/bq25756");
  document.querySelectorAll("button[data-cmd]").forEach((btn) => {
    btn.onclick = () => {
      skipFill = true;
      api(payload(btn.getAttribute("data-cmd")), "/api/bq25756").finally(() => {
        setTimeout(() => { skipFill = false; }, 800);
      });
    };
  });
  document.getElementById("shp-chg-on").onclick = () => api({ cmd: "charge", on: true }, "/api/shp8808");
  document.getElementById("shp-chg-off").onclick = () => api({ cmd: "charge", on: false }, "/api/shp8808");
  document.getElementById("shp-reset").onclick = () => api({ cmd: "reset" }, "/api/shp8808");
  document.getElementById("shp-clear").onclick = () => api({ cmd: "clear_latch" }, "/api/shp8808");
  document.querySelectorAll("button[data-shp-cmd]").forEach((btn) => {
    btn.onclick = () => {
      skipFill = true;
      api(shpPayload(btn.getAttribute("data-shp-cmd")), "/api/shp8808").finally(() => {
        setTimeout(() => { skipFill = false; }, 800);
      });
    };
  });
  setInterval(tick, 500);
  tick();
}

bind();
