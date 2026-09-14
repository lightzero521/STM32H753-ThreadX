const S1 = ["CHG0", "CHG1", "CHG2", "WD", "b4", "VAC_DPM", "IAC_DPM", "ADC_DONE"];
const S2 = ["MPPT0", "MPPT1", "b2", "b3", "TS0", "TS1", "TS2", "PG"];
const S3 = ["b0", "b1", "REVERSE", "CV_TMR", "b4", "b5", "b6", "b7"];
const FAULT = ["b0", "DRV", "SAFETY", "TSHUT", "VBAT_OV", "IBAT_OCP", "VAC_OV", "VAC_UV"];
const F1 = ["CHG", "b1", "b2", "WD", "b4", "VAC_DPM", "IAC_DPM", "ADC_DONE"];
const F2 = ["MPPT", "b1", "b2", "TS", "b4", "b5", "b6", "PG"];
const FF = FAULT;
const KIND = ["FAULT 0x24", "FLAG1 0x25", "FLAG2 0x26", "FAULT_FLAG 0x27"];
const KIND_BITS = [FAULT, F1, F2, FF];

const hist = { t: [], vac: [], vbat: [], iac: [], ibat: [] };
const MAX_PT = 60;
let chart;
let skipFill = false;

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

async function api(body) {
  const opt = body
    ? { method: "POST", headers: { "Content-Type": "application/json" }, body: JSON.stringify(body) }
    : { method: "GET" };
  const r = await fetch("/api/bq25756", opt);
  return r.json();
}

function pushHist(s) {
  const t = (s.uptime_ms / 1000).toFixed(1);
  hist.t.push(t);
  hist.vac.push(s.adc.vac_mv / 1000);
  hist.vbat.push(s.adc.vbat_mv / 1000);
  hist.iac.push(s.adc.iac_x10 / 10000);
  hist.ibat.push(s.adc.ibat_ma / 1000);
  if (hist.t.length > MAX_PT) {
    hist.t.shift();
    hist.vac.shift();
    hist.vbat.shift();
    hist.iac.shift();
    hist.ibat.shift();
  }
  if (!chart) return;
  chart.setOption({
    animation: false,
    grid: { left: 48, right: 48, top: 24, bottom: 28 },
    legend: { data: ["VAC", "VBAT", "IAC", "IBAT"] },
    xAxis: { type: "category", data: hist.t, boundaryGap: false },
    yAxis: [
      { type: "value", name: "V", min: 0 },
      { type: "value", name: "A" }
    ],
    series: [
      { name: "VAC", type: "line", showSymbol: false, data: hist.vac },
      { name: "VBAT", type: "line", showSymbol: false, data: hist.vbat },
      { name: "IAC", type: "line", yAxisIndex: 1, showSymbol: false, data: hist.iac },
      { name: "IBAT", type: "line", yAxisIndex: 1, showSymbol: false, data: hist.ibat }
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

async function tick() {
  try {
    const s = await api(null);
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
    pushHist(s);
  } catch (e) {
    document.getElementById("conn").textContent = "HTTP 失败";
    document.getElementById("conn").className = "bad";
  }
}

function bind() {
  document.querySelectorAll(".drv").forEach((el) => fillSelect(el, ["最快", "较快", "较慢", "最慢"]));
  document.querySelectorAll(".dt").forEach((el) => fillSelect(el, ["45ns", "75ns", "105ns", "135ns"]));
  if (window.echarts) chart = echarts.init(document.getElementById("chart"));
  document.getElementById("btn-chg-on").onclick = () => api({ cmd: "charge", on: true });
  document.getElementById("btn-chg-off").onclick = () => api({ cmd: "charge", on: false });
  document.getElementById("btn-kick").onclick = () => api({ cmd: "kick" });
  document.getElementById("btn-reset").onclick = () => api({ cmd: "reset" });
  document.getElementById("btn-clear").onclick = () => api({ cmd: "clear_latch" });
  document.getElementById("btn-sweep").onclick = () => api({ cmd: "mppt_sweep" });
  document.querySelectorAll("button[data-cmd]").forEach((btn) => {
    btn.onclick = () => {
      skipFill = true;
      api(payload(btn.getAttribute("data-cmd"))).finally(() => {
        setTimeout(() => { skipFill = false; }, 800);
      });
    };
  });
  setInterval(tick, 500);
  tick();
}

bind();
