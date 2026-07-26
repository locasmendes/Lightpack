(() => {
  const leds = [];
  const ring = document.getElementById("led-ring");
  const scene = document.getElementById("screen-scene");
  const frame = document.getElementById("content-frame");
  const frameLabel = document.getElementById("content-label");
  const caption = document.getElementById("stage-caption");
  const chipBlack = document.getElementById("chip-black");
  const homeSub = document.getElementById("home-sub");
  const stageWrap = document.querySelector(".stage-wrap");

  const state = {
    power: true,
    mode: "ambilight",
    aspect: "auto",
    brightness: 72,
    box: { w: 120, h: 80, thick: 6 },
    groups: {
      top: { members: ["side:top"], w: 120, h: 70, inset: 0, enabled: true },
      bottom: { members: ["side:bottom"], w: 140, h: 90, inset: 1, enabled: true },
      sides: { members: ["side:left", "side:right"], w: 90, h: 110, inset: 0, enabled: true },
    },
    activeGroup: "top",
  };

  function sideGroup(side) {
    for (const [name, g] of Object.entries(state.groups)) {
      if (g.members.includes(`side:${side}`)) return { name, ...g };
    }
    return {
      name: "ungrouped",
      w: state.box.w,
      h: state.box.h,
      inset: 0,
      enabled: true,
      members: [],
    };
  }

  // Place LEDs around monitor perimeter (top, right, bottom, left)
  function buildLeds() {
    ring.innerHTML = "";
    leds.length = 0;
    const counts = { top: 14, right: 6, bottom: 14, left: 6 };
    const mk = (side, i, n) => {
      const el = document.createElement("button");
      el.type = "button";
      el.className = "led";
      el.title = `LED ${side} ${i + 1}`;
      const t = (i + 0.5) / n;
      if (side === "top") {
        el.style.left = `${8 + t * 84}%`;
        el.style.top = "0";
      } else if (side === "right") {
        el.style.right = "0";
        el.style.top = `${10 + t * 80}%`;
      } else if (side === "bottom") {
        el.style.right = `${8 + t * 84}%`;
        el.style.bottom = "0";
      } else {
        el.style.left = "0";
        el.style.bottom = `${10 + t * 80}%`;
      }
      el.addEventListener("mouseenter", () => {
        el.style.transform = "scale(1.7)";
        el.style.background = "#fff";
        el.style.color = "#fff";
      });
      el.addEventListener("mouseleave", () => paintLeds());
      ring.appendChild(el);
      leds.push({ el, side, t });
    };
    for (let i = 0; i < counts.top; i++) mk("top", i, counts.top);
    for (let i = 0; i < counts.right; i++) mk("right", i, counts.right);
    for (let i = 0; i < counts.bottom; i++) mk("bottom", i, counts.bottom);
    for (let i = 0; i < counts.left; i++) mk("left", i, counts.left);
  }

  function aspectInsets(aspect) {
    // percentages relative to ultrawide stage
    switch (aspect) {
      case "fill":
        return { top: 0, right: 0, bottom: 0, left: 0, label: "Fill 21:9" };
      case "16:9":
        return { top: 0, right: 14, bottom: 0, left: 14, label: "16:9" };
      case "4:3":
        return { top: 0, right: 24, bottom: 0, left: 24, label: "4:3" };
      case "auto":
      default:
        // pretend detector found 16:9
        return { top: 0, right: 14, bottom: 0, left: 14, label: "Auto → 16:9" };
    }
  }

  function applyAspect() {
    const box = aspectInsets(state.aspect);
    const inset = `${box.top}% ${box.right}% ${box.bottom}% ${box.left}%`;
    scene.style.inset = inset;
    frame.style.inset = inset;
    frameLabel.textContent = box.label;
    caption.textContent = `Content: ${box.label} · monitor 21:9`;

    const sideBars = box.left > 0 || box.right > 0;
    // Show warning only when Fill is forced while "content" would be narrower —
    // for demo: warn when aspect is fill (sampling black bars risk) after auto had bars
    chipBlack.hidden = !(state.aspect === "fill");
    if (!chipBlack.hidden) {
      chipBlack.querySelector("span") || chipBlack;
    }

    homeSub.textContent =
      state.aspect === "auto"
        ? "Blackbar detectou 16:9 — laterais clampadas na borda ativa."
        : state.aspect === "fill"
          ? "Fill usa o painel inteiro. Em filme 16:9 as laterais podem ir para preto."
          : `Preset ${box.label}: content frame centrado no ultrawide.`;

    document.getElementById("mini-content").style.inset = inset;
    layoutMiniZones(box);
    paintLeds();
  }

  function layoutMiniZones(box) {
    const host = document.getElementById("mini-zones");
    host.innerHTML = "";

    const topG = sideGroup("top");
    const bottomG = sideGroup("bottom");
    const leftG = sideGroup("left");
    const rightG = sideGroup("right");

    // thick % drives how far boxes bite into the content; group inset nudges away from bezel
    const thick = state.box.thick;
    const zones = [
      {
        group: topG.name,
        enabled: topG.enabled,
        style: {
          top: `${box.top + topG.inset}%`,
          left: `${box.left}%`,
          width: `${100 - box.left - box.right}%`,
          height: `${Math.max(2, (topG.h / 80) * thick)}%`,
        },
      },
      {
        group: rightG.name,
        enabled: rightG.enabled,
        style: {
          top: `${box.top}%`,
          right: `${box.right + rightG.inset}%`,
          width: `${Math.max(1.5, (rightG.w / 120) * (thick * 0.55))}%`,
          height: `${100 - box.top - box.bottom}%`,
        },
      },
      {
        group: bottomG.name,
        enabled: bottomG.enabled,
        style: {
          bottom: `${box.bottom + bottomG.inset}%`,
          left: `${box.left}%`,
          width: `${100 - box.left - box.right}%`,
          height: `${Math.max(2, (bottomG.h / 80) * thick)}%`,
        },
      },
      {
        group: leftG.name,
        enabled: leftG.enabled,
        style: {
          top: `${box.top}%`,
          left: `${box.left + leftG.inset}%`,
          width: `${Math.max(1.5, (leftG.w / 120) * (thick * 0.55))}%`,
          height: `${100 - box.top - box.bottom}%`,
        },
      },
    ];

    zones.forEach((z) => {
      const el = document.createElement("div");
      el.className = "zone" + (z.enabled ? "" : " is-disabled");
      el.dataset.group = z.group;
      el.title = `Grupo: ${z.group}`;
      Object.assign(el.style, z.style);
      host.appendChild(el);
    });
  }

  function sampleColor(side, t) {
    // Fake scene colors based on gradient stops
    const stops = [
      [30, 140, 220],
      [80, 200, 160],
      [240, 150, 70],
      [200, 80, 120],
    ];
    const idx = t * (stops.length - 1);
    const a = Math.floor(idx);
    const b = Math.min(stops.length - 1, a + 1);
    const f = idx - a;
    const mix = (i) => Math.round(stops[a][i] * (1 - f) + stops[b][i] * f);
    let [r, g, bl] = [mix(0), mix(1), mix(2)];

    // Side bias
    if (side === "left") {
      r = Math.round(r * 0.75);
      bl = Math.min(255, bl + 40);
    }
    if (side === "right") {
      r = Math.min(255, r + 40);
      bl = Math.round(bl * 0.7);
    }
    if (side === "bottom") g = Math.round(g * 0.85);

    // If Fill on UW "film", dim side LEDs (black bars)
    const box = aspectInsets(state.aspect);
    const onBar =
      state.aspect === "fill" &&
      ((side === "left" && t < 1) || (side === "right" && t < 1)) &&
      Math.random() > -1;
    // For fill we simulate wrong sampling: side leds go near black
    if (state.aspect === "fill" && (side === "left" || side === "right")) {
      r = Math.round(r * 0.08);
      g = Math.round(g * 0.08);
      bl = Math.round(bl * 0.1);
    } else if (box.left > 0 && (side === "left" || side === "right")) {
      // clamp policy: sides sample content edge — keep vivid
    }

    const k = state.power ? state.brightness / 100 : 0;
    return [
      Math.round(r * k),
      Math.round(g * k),
      Math.round(bl * k),
    ];
  }

  function paintLeds() {
    let sr = 0,
      sg = 0,
      sb = 0;
    leds.forEach((led) => {
      const [r, g, b] = sampleColor(led.side, led.t);
      const css = `rgb(${r},${g},${b})`;
      led.el.style.background = css;
      led.el.style.color = css;
      sr += r;
      sg += g;
      sb += b;
    });
    const n = Math.max(1, leds.length);
    stageWrap.style.setProperty(
      "--stage-glow",
      `rgba(${Math.round(sr / n)}, ${Math.round(sg / n)}, ${Math.round(sb / n)}, 0.4)`
    );
  }

  // Navigation
  document.querySelectorAll(".nav-list button").forEach((btn) => {
    btn.addEventListener("click", () => {
      document.querySelectorAll(".nav-list button").forEach((b) => b.classList.remove("active"));
      document.querySelectorAll(".panel").forEach((p) => p.classList.remove("active"));
      btn.classList.add("active");
      document.getElementById(btn.dataset.panel).classList.add("active");
    });
  });

  // Power
  const btnPower = document.getElementById("btn-power");
  btnPower.addEventListener("click", () => {
    state.power = !state.power;
    btnPower.textContent = state.power ? "Lights ON" : "Lights OFF";
    btnPower.classList.toggle("off", !state.power);
    paintLeds();
  });

  // Mode
  document.getElementById("mode-seg").addEventListener("click", (e) => {
    const btn = e.target.closest("button[data-mode]");
    if (!btn) return;
    state.mode = btn.dataset.mode;
    [...e.currentTarget.children].forEach((b) => b.classList.toggle("active", b === btn));
    homeSub.textContent =
      state.mode === "ambilight"
        ? "Ambilight: cor das bordas do content frame."
        : state.mode === "mood"
          ? "Mood: lavagem lenta — captura pausada."
          : "Sound: visualizer — captura pausada.";
  });

  // Aspect
  document.getElementById("aspect-seg").addEventListener("click", (e) => {
    const btn = e.target.closest("button[data-aspect]");
    if (!btn) return;
    state.aspect = btn.dataset.aspect;
    [...e.currentTarget.children].forEach((b) => b.classList.toggle("active", b === btn));
    applyAspect();
  });

  // Sliders
  const bind = (id, outId, fmt, on) => {
    const el = document.getElementById(id);
    const out = document.getElementById(outId);
    const sync = () => {
      out.textContent = fmt(el.value);
      on?.(Number(el.value));
    };
    el.addEventListener("input", sync);
    sync();
  };

  bind("brightness", "brightness-val", (v) => `${v}%`, (v) => {
    state.brightness = v;
    paintLeds();
  });
  bind("gamma", "gamma-val", (v) => (Number(v) / 10).toFixed(2));
  bind("smooth", "smooth-val", (v) => `${v} ms`);
  bind("temp", "temp-val", (v) => `${v} K`);
  bind("ob", "ob-val", (v) => `${v}`);

  // —— Global box size ——
  bind("all-w", "all-w-val", (v) => `${v} px`);
  bind("all-h", "all-h-val", (v) => `${v} px`);
  bind("all-thick", "all-thick-val", (v) => `${v}%`, (v) => {
    state.box.thick = v;
    layoutMiniZones(aspectInsets(state.aspect));
  });

  document.getElementById("btn-apply-all-size").addEventListener("click", () => {
    const w = Number(document.getElementById("all-w").value);
    const h = Number(document.getElementById("all-h").value);
    const thick = Number(document.getElementById("all-thick").value);
    state.box = { w, h, thick };
    Object.values(state.groups).forEach((g) => {
      g.w = w;
      g.h = h;
    });
    loadGroupEditor();
    layoutMiniZones(aspectInsets(state.aspect));
  });

  document.getElementById("btn-reset-all-size").addEventListener("click", () => {
    state.box = { w: 120, h: 80, thick: 6 };
    document.getElementById("all-w").value = 120;
    document.getElementById("all-h").value = 80;
    document.getElementById("all-thick").value = 6;
    document.getElementById("all-w-val").textContent = "120 px";
    document.getElementById("all-h-val").textContent = "80 px";
    document.getElementById("all-thick-val").textContent = "6%";
    Object.values(state.groups).forEach((g) => {
      g.w = 120;
      g.h = 80;
      g.inset = 0;
    });
    loadGroupEditor();
    layoutMiniZones(aspectInsets(state.aspect));
  });

  // —— Groups ——
  const groupTabs = document.getElementById("group-tabs");
  const groupMembers = document.getElementById("group-members");

  function renderGroupTabs() {
    groupTabs.innerHTML = "";
    Object.keys(state.groups).forEach((name) => {
      const btn = document.createElement("button");
      btn.type = "button";
      btn.textContent = name;
      btn.dataset.group = name;
      if (name === state.activeGroup) btn.classList.add("active");
      btn.addEventListener("click", () => {
        state.activeGroup = name;
        renderGroupTabs();
        loadGroupEditor();
      });
      groupTabs.appendChild(btn);
    });
  }

  function loadGroupEditor() {
    const g = state.groups[state.activeGroup];
    if (!g) return;
    [...groupMembers.options].forEach((opt) => {
      opt.selected = g.members.includes(opt.value);
    });
    document.getElementById("grp-w").value = g.w;
    document.getElementById("grp-h").value = g.h;
    document.getElementById("grp-inset").value = g.inset;
    document.getElementById("grp-enabled").checked = g.enabled;
    document.getElementById("grp-w-val").textContent = `${g.w} px`;
    document.getElementById("grp-h-val").textContent = `${g.h} px`;
    document.getElementById("grp-inset-val").textContent = `${g.inset}%`;
  }

  bind("grp-w", "grp-w-val", (v) => `${v} px`);
  bind("grp-h", "grp-h-val", (v) => `${v} px`);
  bind("grp-inset", "grp-inset-val", (v) => `${v}%`);

  document.getElementById("btn-create-group").addEventListener("click", () => {
    const raw = document.getElementById("group-name").value.trim().toLowerCase();
    const name = raw.replace(/[^a-z0-9_-]+/g, "-").replace(/^-|-$/g, "");
    if (!name) {
      document.getElementById("group-name").focus();
      return;
    }
    if (state.groups[name]) {
      state.activeGroup = name;
    } else {
      state.groups[name] = {
        members: [],
        w: state.box.w,
        h: state.box.h,
        inset: 0,
        enabled: true,
      };
      state.activeGroup = name;
    }
    document.getElementById("group-name").value = "";
    renderGroupTabs();
    loadGroupEditor();
  });

  document.getElementById("btn-apply-group").addEventListener("click", () => {
    const g = state.groups[state.activeGroup];
    if (!g) return;
    g.members = [...groupMembers.selectedOptions].map((o) => o.value);
    g.w = Number(document.getElementById("grp-w").value);
    g.h = Number(document.getElementById("grp-h").value);
    g.inset = Number(document.getElementById("grp-inset").value);
    g.enabled = document.getElementById("grp-enabled").checked;
    layoutMiniZones(aspectInsets(state.aspect));
  });

  document.getElementById("btn-delete-group").addEventListener("click", () => {
    const names = Object.keys(state.groups);
    if (names.length <= 1) return;
    delete state.groups[state.activeGroup];
    state.activeGroup = Object.keys(state.groups)[0];
    renderGroupTabs();
    loadGroupEditor();
    layoutMiniZones(aspectInsets(state.aspect));
  });

  // Live preview while dragging group sliders
  ["grp-w", "grp-h", "grp-inset"].forEach((id) => {
    document.getElementById(id).addEventListener("input", () => {
      const g = state.groups[state.activeGroup];
      if (!g) return;
      g.w = Number(document.getElementById("grp-w").value);
      g.h = Number(document.getElementById("grp-h").value);
      g.inset = Number(document.getElementById("grp-inset").value);
      layoutMiniZones(aspectInsets(state.aspect));
    });
  });
  document.getElementById("grp-enabled").addEventListener("change", (e) => {
    const g = state.groups[state.activeGroup];
    if (!g) return;
    g.enabled = e.target.checked;
    layoutMiniZones(aspectInsets(state.aspect));
  });

  // Animate scene hues gently
  let hue = 0;
  function tick() {
    hue = (hue + 0.15) % 360;
    if (state.power && state.mode === "ambilight") {
      scene.style.filter = `hue-rotate(${hue}deg) saturate(1.05)`;
      paintLeds();
    }
    requestAnimationFrame(tick);
  }

  buildLeds();
  renderGroupTabs();
  loadGroupEditor();
  applyAspect();
  requestAnimationFrame(tick);
})();
