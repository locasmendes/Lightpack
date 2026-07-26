# Documentação técnica — Prismatik / Lightpack

Índice dos documentos de análise e design produzidos para este fork.

| Documento | Sobre |
|-----------|--------|
| [pipeline-captura-processamento-leds.md](./pipeline-captura-processamento-leds.md) | Fluxo completo: captura → média → device → firmware → LED (com diagramas) |
| [gargalos-sistema-moderno-2026.md](./gargalos-sistema-moderno-2026.md) | Onde estão os gargalos num PC/monitor de 2026 |
| [captacao-cor-ainda-moderna.md](./captacao-cor-ainda-moderna.md) | O método de captação/cálculo de cor ainda é moderno? |
| [pesquisa-zonas-led-content-aware.md](./pesquisa-zonas-led-content-aware.md) | Zonas/caixas LED, ultrawide, ARs múltiplos e propostas content-aware |
| [redesign-ui-prismatik.md](./redesign-ui-prismatik.md) | Reimaginação da UI (princípios, IA, mapa legado→novo) |
| [prototypes/prismatik-ui/index.html](./prototypes/prismatik-ui/index.html) | Protótipo interativo da nova Home / Geometry / Look |
| [ci-build-release.md](./ci-build-release.md) | CI real (6 jobs, zero testes), empacotamento por plataforma, versionamento manual e o mecanismo de auto-update |
| [cobertura-testes.md](./cobertura-testes.md) | O que `LightpackTests` cobre de fato vs. o pipeline crítico sem nenhum teste |
| [ecossistema-plugin-api.md](./ecossistema-plugin-api.md) | Protocolo `ApiServer`, autenticação, como "plugins" funcionam de verdade, e os 11 clientes de exemplo |
| [firmware-hardware-datados.md](./firmware-hardware-datados.md) | MCU/LUFA/hardware original vs. o que a comunidade DIY (WLED/ESP32) já usa hoje, incluindo Arduino+Adalight/FastLED |
| [plans/](./plans/README.md) | 5 planos de implementação acionáveis (checklists + testes) derivados dos achados acima |

## Leitura sugerida

1. Entender o pipeline atual → **pipeline**
2. Ver o que limita responsividade → **gargalos**
3. Avaliar qualidade/modernidade da cor → **captação**
4. Resolver o atrito de configs no ultrawide → **zonas content-aware**
5. Reimaginar a interface → **redesign** + abrir o **protótipo**
6. Entender o que garante qualidade hoje (CI, testes, segurança da API) → **ci-build-release** + **cobertura-testes** + **ecossistema-plugin-api**
7. Avaliar o firmware/hardware original frente ao ecossistema DIY atual → **firmware-hardware-datados**
8. Colocar a mão na massa → **plans/** (planos prontos com checklist e testes)
