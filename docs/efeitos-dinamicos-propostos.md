# Efeitos dinâmicos propostos (pós-5.17)

Documento de produto/engenharia — **só escrita**. Nenhuma implementação aqui.  
Origem: Fase 6 do plano Prismatik **5.17.0.0** (pipeline float linear, calibração, UI revisada).

Objetivo: listar efeitos que **aproveitam o que a release habilita**, em vez de só “mais um chase” no Mood Lamp atual.

---

## 1. Base atual — 8 lâmpadas via `DECLARE_LAMP`

Registro e factory em `Software/src/MoodLamp.cpp` (macro `DECLARE_LAMP` na linha ~72). Cada lamp implementa `shine(const QColor&, QList<QRgb>&)` e opcionalmente `init()` / `interval()`.

| Nome na UI | Classe gerada | Linha aprox. | Comportamento resumido | Params (via `MoodLampManager::visibleEffectParamsForLamp`) |
|---|---|---|---|---|
| **Static (default)** | `StaticMoodLamp` | ~136 | Cor uniforme em todos os LEDs habilitados | — |
| **Fire** | `FireMoodLamp` | ~153 | Simulação estilo FastLED Fire2012 (cooling + sparks + HSL lightness) | — |
| **RGB is Life** | `RGBLifeMoodLamp` | ~228 | Rotação de hue da cor escolhida ao longo da faixa | — (Speed hardcoded) |
| **Breathing** | `BreathingMoodLamp` | ~254 | Pulso senoidal de lightness; forçado no color mode Breathing | — |
| **Rainbow** | `RainbowMoodLamp` | ~280 | Espectro HSV completo na faixa, ignora a cor do usuário | Speed, Direction |
| **Comet** | `CometMoodLamp` | ~317 | Cabeça brilhante + trail com fade exponencial | Speed, Direction |
| **Theater Chase** | `TheaterChaseMoodLamp` | ~369 | Pontos espaçados em marquee (a cada 3 LEDs) | Speed, Direction |
| **Twinkle** | `TwinkleMoodLamp` | ~403 | Pixels esparsos sobem a 255 e decaem | Speed, Density |

Infra já existente e reutilizável:

- Params por lamp: `Settings::getMoodLampEffectSpeed/Density/Direction` + UI contextual (`visibleEffectParamsForLamp`, `MoodLampManager.cpp` ~277).
- Overrides de cor por grupo **só no Constant mode** (`MoodLampManager::updateColors` ~199–205).
- Transporte hoje: `QList<QRgb>` 8-bit; a Fase 2 da 5.17 move a cadeia primária para `QList<LinearRgbF>` (MoodLamp/Sound incluídos no transporte, **sem** passar pelo estágio de conteúdo sat/contraste — ver plano §2.7 / R13).

Limitações estruturais que os efeitos abaixo exploram ou quebram de propósito:

1. **Espaço de cor 8-bit + HSL/HSV de Qt** — banding em gradientes lentos; fades de brilho não são perceptualmente uniformes.
2. **Modos mutuamente exclusivos** — Ambilight / Mood Lamp / Sound Viz; só um `start(true)` por vez (`LightpackApplication.cpp` ~317–344; índices em `SettingsWindow.cpp` ~108–113).
3. **Geometria linear por índice de LED** — Fire/Comet/Rainbow tratam a faixa como array 1D; não leem `LedGroup::Edge` (`Top`/`Bottom`/`Left`/`Right`/`Custom` em `Settings.hpp`).
4. **Áudio só no modo Sound** — `SoundManagerBase` expõe FFT (`fft()`, `fftSize()`) e visualizers, mas o grab e o mood lamp estão desligados enquanto ele roda.

---

## 2. Mudança arquitetural candidata: layering sobre o grab

Hoje “efeito reativo sobre o ambilight” **não existe** sem redesenhar o orquestrador de modos. Documentar o custo com honestidade (o plano da 5.17 deixa layering **fora de escopo de implementação**; este doc só mapeia o caminho).

### 2.1 Estado atual

```
AmbilightMode  → GrabManager ON,  MoodLamp OFF, Sound OFF
MoodLampMode   → GrabManager OFF, MoodLamp ON,  Sound OFF
SoundVizMode   → GrabManager OFF, MoodLamp OFF, Sound ON
```

Cada manager emite `updateLedsColors` para o mesmo `LedDeviceManager`. Não há compositor.

### 2.2 Modelo proposto (pós-5.17, release seguinte ou feature gated)

| Camada | Papel | Domínio sugerido |
|---|---|---|
| **Base** | Cores do grab (pós-`ColorPipeline` conteúdo B1–B4) | `LinearRgbF` |
| **Modulação** | Efeito (mood / áudio / máscara de grupo) | multiplicativo / mix em linear ou encoded, conforme o efeito |
| **Saída** | `SmoothingDriver` + device (D1–D6) | linear → wire |

Opções de composição (escolher uma na implementação futura):

- **Multiply / screen** — efeito só escala lightness ou saturação do ambilight (bom para ember, vignette, bass punch).
- **Mix α** — `out = (1−α)·grab + α·effect` com α global ou por LED/grupo.
- **Máscara espacial** — efeito só em grupos `Edge::Left|Right` (bordas), grab no resto.

### 2.3 Custo honesto

| Item | Estimativa | Nota |
|---|---|---|
| Orquestração (ligar Grab + camada de efeito ao mesmo tempo) | M–G | Quebra o `switch` de modos; UI “Cena” precisa de toggle “efeito sobre ambilight”; settings novos |
| Compositor + testes | M | Golden cases: α=0 ≡ grab; α=1 ≡ efeito puro; multiply neutro = identidade |
| Latência / CPU | Baixo | ~100 LEDs; float já orçado na Fase 2 (&lt;1% core) |
| Threading | Baixo se feito certo | Manter `QueuedConnection`; compor **antes** do device thread, no host (Grab/Mood/Sound → um único emit) |
| Semântica de FPS | M | Mood timer (~33 ms) vs grab (~50 ms default) — precisa clock comum ou “efeito amostrado no tick do grab” |
| Risco de produto | Alto | Usuários esperam modos exclusivos há anos; feature gated + default off |

**Veredito:** layering é pré-requisito de vários efeitos “interessantes” abaixo; **não** cabe como “só mais um `DECLARE_LAMP`”. Tratar como épico separado, dependente do transporte float e do `SmoothingDriver` unificado da 5.17.

---

## 3. Capacidades da 5.17 que destravam efeitos novos

| Capacidade | Onde no plano | O que libera para efeitos |
|---|---|---|
| Working space float linear (`LinearRgbF` / `EncodedRgbF` / `WireRgbF`) | Fase 2 | Gradientes sem requantizar 5×; fades Lab/perceptuais; mix sem banding |
| `SmoothingDriver` compartilhado | Fase 2 §2.7 | Transições suaves entre estados de efeito sem duplicar timers |
| Preview live pós-pipeline | Fase 4 | Desenvolver/validar efeitos vendo o output real nas zonas |
| `LedGroup` + `LedGroupRuntime` | Já em master; UI na Fase 5 | Ondas e máscaras por aresta (top/bottom/sides) |
| `SoundManagerBase` + FFT | Já existe (`SOUNDVIZ_SUPPORT`) | Reatividade a banda sem reinventar captura de áudio |
| UI “Cena” unificada | Fase 5 | Lugar natural para “modo + camada” sem três páginas exclusivas |

---

## 4. Efeitos propostos

Esforço: **P** (≤2 dias, cabe em `DECLARE_LAMP` + params), **M** (3–8 dias, geometria/áudio ou float), **G** (épico / mudança de arquitetura).

### 4.1 Soft Gradient (Lab)

| | |
|---|---|
| **O que é** | Duas cores (ou cor do usuário → cor secundária) interpoladas ao longo da faixa em espaço Lab (ou encoded float com OETF), animação lenta de fase. |
| **Por que interessante** | Substitui o look “degrau” de Rainbow/RGB is Life em TVs escuras; vitrine direta do float linear. |
| **Esforço** | M |
| **Dependências 5.17** | `LinearRgbF` + ops Lab já existentes em `PrismatikMath` (usadas em float, sem clamp 8-bit intermediário). Transporte float no MoodLamp. |
| **Layering?** | Não obrigatório — funciona como lamp puro. |

### 4.2 Perceptual Breath

| | |
|---|---|
| **O que é** | Evolução do Breathing: pulso de luminosidade em domínio perceptual (L\* ou encoded lerp), não `QColor::setHsl(..., lightness * scale)` em 8-bit. |
| **Por que interessante** | Breathing atual “acha” no meio e “corta” perto do preto por quantização; vitrine de fade uniforme. |
| **Esforço** | P–M |
| **Dependências 5.17** | Float + (opcional) `SmoothingDriver` para trocas de cor Liquid. |
| **Layering?** | Não. |

### 4.3 Ember Overlay (grab × fogo)

| | |
|---|---|
| **O que é** | Campo de “brasas” (reuso da lógica de lightness do Fire) **multiplicado** sobre as cores do ambilight ao vivo — a imagem continua reconhecível, as bordas “crepitam”. |
| **Por que interessante** | Primeiro efeito que só faz sentido **sobre** o grab; diferencial vs Hyperion/WLED “replace mode”. |
| **Esforço** | G (inclui compositor) + M no efeito em si |
| **Dependências 5.17** | Pipeline float (modulação em linear); preview Fase 4; **layering** (§2). |
| **Layering?** | **Sim — bloqueante.** |

### 4.4 Reactive Twinkle

| | |
|---|---|
| **O que é** | Twinkles só nascem (ou nascem com peso maior) onde o grab já está acima de um limiar de luminância; decay em float. |
| **Por que interessante** | “Estrelas” que acompanham highlights da cena (explosões, UI branca) sem apagar o ambilight. |
| **Esforço** | G (layering) + P no gerador |
| **Dependências 5.17** | Float; layering; opcionalmente histerese do pipeline para não flicker em limiar. |
| **Layering?** | **Sim.** |

### 4.5 Edge Wave (grupos)

| | |
|---|---|
| **O que é** | Fronte de onda que percorre grupos na ordem Top → Right → Bottom → Left (ou o inverso), usando `LedGroup::Edge` + `memberIds` ordenados ao longo da aresta — não o índice global cru da faixa. |
| **Por que interessante** | Layouts reais não são um anel perfeito em índice 0..N; Comet/Theater Chase “quebram” em cantos. Grupos já modelam a topologia. |
| **Esforço** | M |
| **Dependências 5.17** | `LedGroupRuntime` / `Settings::getLedGroups()` (já master); UI Geometria (Fase 5) para o usuário ter grupos confiáveis. Float opcional mas desejável no trail. |
| **Layering?** | Não obrigatório; variante overlay (§4.3) seria G. |

### 4.6 Cinema Vignette (grupos + grab)

| | |
|---|---|
| **O que é** | Escurece progressivamente LEDs de `Edge::Left`/`Right` (e opcionalmente Top) enquanto o centro/arestas restantes seguem o grab — “letterbox vivo”. |
| **Por que interessante** | Une content-aware informal (pretos laterais) com ambiência; casa com presets de AR da Geometria. |
| **Esforço** | G (layering + máscara por grupo) |
| **Dependências 5.17** | Layering; grupos; float para rampas suaves de vignette. |
| **Layering?** | **Sim.** |

### 4.7 Spectrum Cascade (áudio + grupos)

| | |
|---|---|
| **O que é** | Mapeia bandas da FFT de `SoundManagerBase` para arestas: graves → Bottom, médios → Left/Right, agudos → Top (configurável). Cores via min/max color já existentes no sound viz. |
| **Por que interessante** | Visualizers atuais pintam a faixa 1D; com grupos a sala “sente” o espectro no perímetro correto. |
| **Esforço** | M |
| **Dependências 5.17** | `SoundManagerBase::fft()` (já); grupos; transporte float (Fase 2) para suavizar barras sem banding. UI Cena (Fase 5) se deixar de ser modo exclusivo. |
| **Layering?** | Não para v1 (substitui grab como hoje). Variante “bass punch sobre ambilight” → G + §2. |

### 4.8 Bass Punch Overlay

| | |
|---|---|
| **O que é** | Envelope de baixa frequência da FFT escala o brilho (ou exposição linear B4-like) do frame de grab — kick = flush de luz, silêncio = ambilight normal. |
| **Por que interessante** | Gaming/filmes com trilha: ambilight continua sendo a estrela; áudio só “respira”. |
| **Esforço** | G |
| **Dependências 5.17** | Layering; `SoundManagerBase` **e** Grab ligados; float (ganho em linear). Orquestração nova: sound deixa de ser modo exclusivo e vira fonte de envelope. |
| **Layering?** | **Sim — bloqueante.** Também exige “áudio como sidechain”, não só como painter. |

### 4.9 Dual-Comet Lab

| | |
|---|---|
| **O que é** | Dois cometas em sentidos opostos com trails em float/Lab; colisão = mix aditivo clamado em linear. |
| **Por que interessante** | Upgrade visual barato do Comet atual; mostra ganho do working space sem arquitetura nova. |
| **Esforço** | P–M |
| **Dependências 5.17** | Transporte float no MoodLamp; params Speed/Direction já no padrão Comet. |
| **Layering?** | Não. |

### 4.10 Group Color Morph

| | |
|---|---|
| **O que é** | Em Constant/Liquid, as cores `LedGroup::color` interpolam no tempo (Lab) entre presets ou liquid generator — em vez do override estático de hoje. |
| **Por que interessante** | Grupos já têm `hasColor`; hoje só aplicam em Constant (`MoodLampManager` ~199–205). Animar isso vira “zonas de humor” (topo frio, base quente). |
| **Esforço** | M |
| **Dependências 5.17** | Float/Lab; grupos; possível extensão de `applyGroupColorOverrides` (plano §2.7 já cita extrair como função livre). |
| **Layering?** | Não (mood puro). Variante sobre grab = G. |

---

## 5. Matriz resumida

| Efeito | Categoria | Esforço | Float linear | Layering | Grupos | Áudio |
|---|---|---|---|---|---|---|
| Soft Gradient (Lab) | Mood puro | M | ✓ | — | — | — |
| Perceptual Breath | Mood puro | P–M | ✓ | — | — | — |
| Dual-Comet Lab | Mood puro | P–M | ✓ | — | — | — |
| Edge Wave | Mood + geometria | M | desejável | — | ✓ | — |
| Group Color Morph | Mood + geometria | M | ✓ | — | ✓ | — |
| Spectrum Cascade | Sound + geometria | M | desejável | — | ✓ | ✓ |
| Ember Overlay | Reativo | G | ✓ | ✓ | opcional | — |
| Reactive Twinkle | Reativo | G | ✓ | ✓ | — | — |
| Cinema Vignette | Reativo + geometria | G | ✓ | ✓ | ✓ | — |
| Bass Punch Overlay | Reativo + áudio | G | ✓ | ✓ | opcional | ✓ |

---

## 6. Ordem sugerida de implementação (futuro)

1. **Quick wins pós-5.17 (sem layering):** Perceptual Breath, Dual-Comet Lab, Soft Gradient — validam float na prática do Mood Lamp.
2. **Geometria:** Edge Wave, Group Color Morph, Spectrum Cascade — exigem grupos bem preenchidos na UI.
3. **Épico de layering** (release própria): compositor + settings + UI Cena → depois Ember, Reactive Twinkle, Cinema Vignette, Bass Punch.

Não implementar nenhum destes nesta Fase 6; não misturar com o escopo de código da 5.17.0.0.

---

## 7. Referências de código

- `Software/src/MoodLamp.cpp` — `DECLARE_LAMP`, oito efeitos.
- `Software/src/MoodLampManager.cpp` — timer, Constant/Breathing/Liquid, `visibleEffectParamsForLamp`, group color overrides.
- `Software/src/LightpackApplication.cpp` — exclusão mútua dos modos (~317–344).
- `Software/src/SettingsWindow.cpp` — `GrabModeIndex` / `MoodLampModeIndex` / `SoundVisualizeModeIndex` (~108–113).
- `Software/src/Settings.hpp` — `struct LedGroup` + `Edge`.
- `Software/src/LedGroupRuntime.{hpp,cpp}` — apply de overrides de geometria/cor.
- `Software/src/SoundManagerBase.{hpp,cpp}` — FFT e emit de cores do visualizer.
- Plano 5.17 — Fase 2 (float), Fase 5 (UI Cena), “Fora de escopo: layering Mood sobre grab”.
