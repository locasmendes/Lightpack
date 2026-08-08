# Phase 1 — Checklist manual multi-tela (Prismatik 5.17)

Pré-requisitos: build da branch `phase-1-multiscreen`, 2 monitores, device Adalight (ou Virtual) ligado, zonas configuradas só em **um** dos monitores.

## Cenários

| # | Ação | Resultado esperado |
|---|---|---|
| (a) | Desplugar o monitor **sem** zonas | Zonas do outro monitor **não se movem**; LEDs continuam |
| (b) | Desplugar o monitor **com** as zonas | LEDs **apagaram** (salvo `Main/IsKeepLightsOnAfterScreenDisconnect=true`); zonas permanecem nas coordenadas salvas no perfil (não “andam” sozinhas) |
| (c) | Replugar o monitor das zonas | LEDs **voltam** (se tinham sido apagados em b); zonas nas posições originais do perfil |
| (d) | Desligar o monitor das zonas pelo botão (sem desconectar cabo) | Mesmo comportamento de (b)/(c) — sleep de display / topologia |
| (e) | Mudar resolução do monitor **com** zonas | Zonas **não deslocam**; após ~300 ms voltam às posições salvas |

## Regressão

| # | Ação | Resultado esperado |
|---|---|---|
| (r1) | Uma tela só; resize da janela / mudança de resolução | Comportamento estável; sem crash; zonas restauradas do perfil, sem translação “fantasma” |
| (r2) | Abrir config de zona cujas coordenadas estão fora de qualquer tela | Sem crash (`GrabConfigWidget` null-guard) |

## Setting

- `Main/IsKeepLightsOnAfterScreenDisconnect` (default `false`): se `true`, LEDs **não** apagam no disconnect da tela das zonas.
- Perfil: `Grab/ZoneScreenIdentity` = `name|manufacturer|serial` da tela que concentra as zonas.
