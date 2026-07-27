# Planos de implementação

Planos acionáveis, cada um transformando um achado já documentado em `docs/*.md` em um checklist rastreável com testes, verificado contra o código-fonte antes de ser salvo. Os 5 primeiros foram escritos com apoio do Codex (delegado via `codex:codex-rescue`); o 6º (`grupos-e-resize-global-leds.md`) foi escrito depois, já com os 3 primeiros implementados e mergeados.

| Plano | Área | Baseado em | Itens | Testes | Status |
|---|---|---|---|---|---|
| [smoothing-host-side.md](./smoothing-host-side.md) | Suavização temporal genérica no host — slider Off↔400ms para devices sem smoothing nativo (Adalight/Ardulight/UDP) | [`gargalos-sistema-moderno-2026.md`](../gargalos-sistema-moderno-2026.md) §3.2, [`firmware-hardware-datados.md`](../firmware-hardware-datados.md) §3.1 | 33 | ✅ | Implementado (`master`) |
| [cobertura-testes-implementacao.md](./cobertura-testes-implementacao.md) | Rodar `LightpackTests` no CI + testes novos para `PrismatikMath` (Lab/XYZ) e `AbstractLedDevice::applyColorModifications` | [`cobertura-testes.md`](../cobertura-testes.md) §7 | 25 | ✅ | Não implementado |
| [ci-release-hygiene.md](./ci-release-hygiene.md) | Testes no CI, check de sincronismo de versão (`RELEASE_VERSION` vs `version.h`), decisão sobre `.travis.yml` vs `ci.yml` | [`ci-build-release.md`](../ci-build-release.md) | 15 | ✅ | Não implementado |
| [presets-aspect-ratio.md](./presets-aspect-ratio.md) | Presets de AR (`Fill`/`16:9`/`4:3`) sobre o mesmo layout físico de LEDs, sem duplicar perfil inteiro (item Q1) | [`pesquisa-zonas-led-content-aware.md`](../pesquisa-zonas-led-content-aware.md) §5.1 | 39 | ✅ | Implementado (`master`) |
| [suporte-protocolo-ddp.md](./suporte-protocolo-ddp.md) | Novo `LedDeviceDdp`, seguindo o padrão de WARLS/DRGB/DNRGB | [`firmware-hardware-datados.md`](../firmware-hardware-datados.md) §3.3 | 22 | ✅ | Implementado (`master`) |
| [grupos-e-resize-global-leds.md](./grupos-e-resize-global-leds.md) | Resize em lote de todas as caixinhas + grupos nomeados de LEDs (`top`/`bottom`/custom) com overrides próprios, sem arrastar caixa por caixa | [`redesign-ui-prismatik.md`](../redesign-ui-prismatik.md) §4.2/§6 | 20 | — | Não implementado |

## Como usar

Cada plano é independente — não há ordem obrigatória entre eles, exceto onde um plano cita outro como pré-requisito (ex.: rodar testes no CI antes de expandir a suíte). Dentro de cada plano, siga as fases em ordem: cada uma foi desenhada para deixar o repositório num estado consistente antes da próxima começar.

Para marcar progresso, edite o próprio arquivo do plano trocando `- [ ]` por `- [x]` conforme os itens forem implementados e testados.

## Escopo

Estes 6 planos cobrem os achados com maior relação esforço/impacto já identificados nesta pasta — não são os únicos possíveis. Itens de maior esforço ficaram de fora deliberadamente (ex.: blackbar detection real, pipeline HDR/float completo, migração de firmware para ESP32) — ver as seções de "roadmap"/"matriz de decisão" dos respectivos docs de pesquisa se quiser planejá-los depois.
