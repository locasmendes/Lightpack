# Planos de implementação

Cinco planos acionáveis, cada um transformando um achado já documentado em `docs/*.md` em um checklist rastreável com testes. Escritos com apoio do Codex (delegado via `codex:codex-rescue`), verificados contra o código-fonte antes de serem salvos.

| Plano | Área | Baseado em | Itens | Testes |
|---|---|---|---|---|
| [smoothing-host-side.md](./smoothing-host-side.md) | Suavização temporal genérica no host — slider Off↔400ms para devices sem smoothing nativo (Adalight/Ardulight/UDP) | [`gargalos-sistema-moderno-2026.md`](../gargalos-sistema-moderno-2026.md) §3.2, [`firmware-hardware-datados.md`](../firmware-hardware-datados.md) §3.1 | 33 | ✅ |
| [cobertura-testes-implementacao.md](./cobertura-testes-implementacao.md) | Rodar `LightpackTests` no CI + testes novos para `PrismatikMath` (Lab/XYZ) e `AbstractLedDevice::applyColorModifications` | [`cobertura-testes.md`](../cobertura-testes.md) §7 | 25 | ✅ |
| [ci-release-hygiene.md](./ci-release-hygiene.md) | Testes no CI, check de sincronismo de versão (`RELEASE_VERSION` vs `version.h`), decisão sobre `.travis.yml` vs `ci.yml` | [`ci-build-release.md`](../ci-build-release.md) | 15 | ✅ |
| [presets-aspect-ratio.md](./presets-aspect-ratio.md) | Presets de AR (`Fill`/`16:9`/`4:3`) sobre o mesmo layout físico de LEDs, sem duplicar perfil inteiro (item Q1) | [`pesquisa-zonas-led-content-aware.md`](../pesquisa-zonas-led-content-aware.md) §5.1 | 39 | ✅ |
| [suporte-protocolo-ddp.md](./suporte-protocolo-ddp.md) | Novo `LedDeviceDdp`, seguindo o padrão de WARLS/DRGB/DNRGB | [`firmware-hardware-datados.md`](../firmware-hardware-datados.md) §3.3 | 22 | ✅ |

## Como usar

Cada plano é independente — não há ordem obrigatória entre eles, exceto onde um plano cita outro como pré-requisito (ex.: rodar testes no CI antes de expandir a suíte). Dentro de cada plano, siga as fases em ordem: cada uma foi desenhada para deixar o repositório num estado consistente antes da próxima começar.

Para marcar progresso, edite o próprio arquivo do plano trocando `- [ ]` por `- [x]` conforme os itens forem implementados e testados.

## Escopo

Estes 5 planos cobrem os achados com maior relação esforço/impacto já identificados nesta pasta — não são os únicos possíveis. Itens de maior esforço ficaram de fora deliberadamente (ex.: blackbar detection real, pipeline HDR/float completo, migração de firmware para ESP32) — ver as seções de "roadmap"/"matriz de decisão" dos respectivos docs de pesquisa se quiser planejá-los depois.
