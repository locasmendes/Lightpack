# Plano: resize global e grupos de LEDs

Plano de implementação da seção 4.2/6 de [redesign-ui-prismatik.md](../redesign-ui-prismatik.md): permitir configurar largura/altura/espessura de **todas** as caixinhas (`GrabWidget`) de uma vez, e criar **grupos nomeados** de LEDs (ex.: `top`, `bottom`, `pedestal`) com overrides próprios — sem precisar arrastar caixa por caixa. É um recurso de usabilidade do wizard/editor de zonas, ortogonal ao `presets-aspect-ratio.md` (que resolve *qual área da tela* é amostrada, não *como configurar as caixas* que fazem a amostragem).

## 1. Decisões de design e restrições confirmadas

Estado atual verificado no código:

| Peça | Estado hoje | Fonte |
|------|-------------|-------|
| `GrabWidget` | Só conhece id, coeficientes RGB, enabled e geometria (`pos()`/`size()` do próprio `QWidget`). Nenhum conceito de "grupo". | `Software/src/GrabWidget.hpp:37-153` |
| Redimensionar uma caixa | Só via arraste do mouse (`mousePressEvent`/`mouseMoveEvent`/`resizeAccordingly`, `Software/src/GrabWidget.cpp:177-521`) ou programaticamente via `move()`/`resize()` chamados por `settingsProfileChanged()` ao carregar perfil (`Software/src/GrabWidget.cpp:149-166`). Não existe hoje "redimensionar N caixas de uma vez" em nenhum lugar do código. |
| Hook de agrupamento | `GrabWidget::mouseRightButtonClicked(int selfId)` é emitido em `mousePressEvent` com o comentário literal `// Send signal RightButtonClicked to main window for grouping widgets` (`Software/src/GrabWidget.cpp:192-196`, declarado em `GrabWidget.hpp:80`) — **mas não está conectado a nada em nenhum arquivo do repositório** (`grep -rn "mouseRightButtonClicked" Software/src` só retorna o `emit`). É um hook morto de uma ideia de agrupamento nunca implementada; este plano o reaproveita. |
| Persistência por LED | Só `LED_N/Position`, `LED_N/Size`, `LED_N/IsEnabled`, `LED_N/CoefRed\|Green\|Blue` (`Software/src/Settings.cpp:245-255` namespace `Led`; getters/setters em `Software/src/Settings.cpp` próximos a `getLedPosition`/`setLedPosition`). Nenhuma noção de grupo persistida. |
| `CustomDistributor` | Aceita **uma única** espessura (`_thickness`) e **um único** stand width (`_standWidth`) para o distributor inteiro, aplicados uniformemente a top/side/bottom (`_height = _screen.height() * _thickness` reusado em `startTopRightToLeft`/`startTopLeftToBottom`; `_width = _screen.width() * _thickness` em `startBottomRightToTop`/`startTopLeftToBottom`, `Software/src/wizard/CustomDistributor.cpp:41-139`). Não há espessura diferente por lado. |
| `LayoutRecipeGenerator::MonitorRecipe` | Herdou a mesma limitação do `CustomDistributor`: um `thicknessPercent`/`standWidthPercent` único por monitor (`Software/src/wizard/LayoutRecipeGenerator.hpp`, ver `docs/plans/presets-aspect-ratio.md`). |

Decisões:

| Decisão | Recomendação adotada | Justificativa |
|---|---|---|
| Onde vive o "resize global" | Operação nova, **não** passa por `CustomDistributor`/`LayoutRecipeGenerator` | Redimensionar em lote caixas já existentes é ortogonal a "gerar do zero por fórmula" (Andromeda/Cassiopeia/Pegasus/Apply); reescrever `CustomDistributor` pra aceitar espessura por lado é um refactor de alto risco na classe mais delicada do wizard, desnecessário para o objetivo. |
| Modelo de grupo | Lista nomeada de **IDs de LED** (membership explícita), não "lado" calculado a partir de uma receita | Não depende de `Grab/LayoutRecipe` existir (feature de presets é opcional); funciona igual em perfis manuais e perfis gerados pelo wizard. |
| "Thickness" em nível de grupo | Reinterpretado como **pós-processamento**: resize em lote escopado aos membros do grupo, na dimensão perpendicular à borda que o grupo representa (top/bottom mexe em `height`; left/right mexe em `width`; `Custom` mexe nos dois) | Reaproveita a mesma primitiva do resize global (Fase 2) em vez de duplicar lógica ou tocar `CustomDistributor`. |
| Seleção de membros do grupo na UI | Clique direito numa caixa alterna sua participação no grupo "em edição" | Reaproveita `GrabWidget::mouseRightButtonClicked`, hoje morto, em vez de inventar um mecanismo novo de seleção múltipla. |
| Interação com content-aspect-presets | Trocar de preset (`ZoneLayoutRuntime::applyContentAspectPreset`) **não** reaplica grupos automaticamente nesta primeira versão; overrides de grupo são perdidos ao trocar de preset, mesma decisão já tomada para edições manuais no plano de presets (`docs/plans/presets-aspect-ratio.md` item 47) | Evita prometer composição entre duas camadas de geometria antes de validar cada uma isoladamente; documentar e avisar na UI em vez de inferir comportamento não pedido. |
| Não-objetivos | Sem blackbar detection, sem "seguir janela", sem editor tipo spline (visão D6 do `pesquisa-zonas-led-content-aware.md`), sem inferir membership de grupo a partir de uma receita automaticamente | Fora do escopo incremental deste plano; ver roadmap do redesign doc. |

## 2. Itens rastreáveis

### Fase 1 — motor de resize em lote

- [x] Extrair a matemática pura de "redimensionar mantendo uma âncora" para uma função testável sem `QWidget` real: `QRect resizedKeepingAnchor(const QRect& current, int newWidth, int newHeight, Qt::Corner anchor)` (usar `newWidth <= 0` ou `newHeight <= 0` como "não mexer neste eixo", espelhando o `-1 = unset` de overrides de grupo). Local sugerido: `Software/src/wizard/BulkResize.hpp/.cpp` (nome análogo a `LayoutRecipeGenerator`).
- [ ] Função de aplicação que recebe `QList<GrabWidget*>` + a mesma assinatura de resize, chama `resizedKeepingAnchor` por widget, e então `widget->resize(...)`/`widget->move(...)` seguido de `widget->saveSizeAndPosition()` (já existe, `Software/src/GrabWidget.cpp:139-147`) para persistir cada `LED_N/Position`/`LED_N/Size` alterado (será conectado pela Fase 4).
- [ ] Após aplicar, chamar o mesmo caminho de revalidação que o wizard já usa após qualquer mudança de zonas (`ZonePlacementPage::checkZoneIssues()`, `Software/src/wizard/ZonePlacementPage.cpp:154-213`) — não introduzir uma segunda checagem de sobreposição/gap (será conectado pela Fase 4).

### Fase 2 — modelo de grupo e persistência

- [x] Struct `LedGroup { QString name; QList<int> memberIds; enum class Edge { Top, Bottom, Left, Right, Custom } edge; int width = -1; int height = -1; bool enabled = true; }` — `width`/`height` = `-1` significa "sem override, preserva o valor atual da caixa".
- [x] `SettingsScope::Profile::Key::Grab::LedGroups = "Grab/LedGroups"`, persistido como JSON (mesmo padrão de `Grab/LayoutRecipe`, `Software/src/Settings.cpp`, ver `docs/plans/presets-aspect-ratio.md` Fase 2), default lista vazia — perfil sem grupos definidos é 100% equivalente ao comportamento atual (feature opt-in).
- [x] `Settings::getLedGroups()`/`setLedGroups(...)` + sinal de mudança, seguindo exatamente o padrão de `getLayoutRecipe()`/`setLayoutRecipe(...)`.
- [x] Validar ao carregar: `memberIds` fora do intervalo `[0, numberOfLeds)` do perfil atual são ignorados silenciosamente na aplicação (Fase 3), não removidos da persistência (o perfil pode ter menos LEDs temporariamente por edição em andamento) — nunca deixar um ID inválido travar a aplicação do grupo inteiro.

### Fase 3 — aplicação runtime (ponto único, dentro e fora do wizard)

- [x] `Software/src/LedGroupRuntime.hpp/.cpp` (mesmo nível de `ZoneLayoutRuntime`, não em `wizard/`, pois roda também fora do wizard): `static bool LedGroupRuntime::applyGroup(const LedGroup& group)` — resolve `memberIds` válidos do perfil atual direto via `Settings::setLedPosition/setLedSize` (quando chamado fora do wizard, sem `GrabWidget`s instanciados; dentro do wizard os GrabWidgets sincronizam via Settings) usando a mesma direção por `edge` da Fase 1.
- [x] `static bool LedGroupRuntime::applyAll()` — aplica todos os grupos habilitados do perfil atual, na ordem em que foram criados; grupos com membros sobrepostos aplicam em ordem, o último grupo que toca um LED vence (documentar explicitamente, não é erro).
- [x] Reaproveitar exatamente o mesmo ponto de entrada tanto da UI do wizard (Fase 4) quanto de um botão em `SettingsWindow` fora do wizard (não duplicar a lógica de resolução de membros/edge entre os dois contextos).

### Fase 4 — UI no wizard (`ZonePlacementPage`)

- [ ] Grupo de controles "Resize all" (spinboxes largura/altura + botão aplicar) na página de zonas, chamando a Fase 1 sobre `_screens[_ui->cbMonitorSelect->currentIndex()].grabAreas` inteiro (`Software/src/wizard/ZonePlacementPage.hpp:84-107` para a estrutura `MonitorSettings`/`grabAreas` já existente).
- [ ] Painel de grupos: criar grupo nomeado + combo de `edge`; conectar `GrabWidget::mouseRightButtonClicked` (hoje não conectado, `Software/src/GrabWidget.hpp:80`) a um slot que alterna a caixa clicada dentro/fora do grupo "em edição no momento", com feedback visual (ex.: borda destacada) enquanto o painel de grupo está aberto.
- [ ] Botão "Aplicar grupo" chama `LedGroupRuntime::applyGroup` (Fase 3) e depois persiste a definição do grupo via `Settings::setLedGroups` (Fase 2) — não persistir o grupo antes de o usuário confirmar a aplicação.
- [ ] Listar grupos já existentes do perfil (nome + edge + contagem de membros), com opção de editar/remover; remover um grupo não desfaz o resize já aplicado aos LEDs (mesma filosofia de "reaplicar layout canônico" do plano de presets, item 47).

### Fase 5 — UI fora do wizard / API (opcional, escopo posterior)

- [ ] Botão "Reaplicar grupos de LEDs" em `SettingsWindow` chamando `LedGroupRuntime::applyAll()` sem precisar reabrir o wizard.
- [ ] Comandos de API (`getledgroups`/`setledgroup:...`) só depois de validar a sintaxe de um grupo (nome, edge, lista de membros, width, height) num protocolo de texto simples sem ambiguidade — se a sintaxe ficar complexa demais, adiar para um plano futuro em vez de forçar um formato ruim (mesma cautela já registrada para `getcontentaspect` em `docs/plans/presets-aspect-ratio.md` item 62).

#### 5.1 Design do protocolo de API (decidido, ainda não implementado)

Sintaxe escolhida seguindo o mesmo estilo já usado em `ApiServer.cpp` (campos separados por `,` dentro de uma entrada, entradas separadas por `;`, ver `CmdGetLeds`/`CmdResultLeds` — formato `id-x,y,w,h;` — e valores simples em minúsculas como em `CmdSetContentAspect` — `fill`/`16:9`/`4:3`):

| Comando | Direção | Formato | Exemplo |
|---|---|---|---|
| `getledgroups` | pedido (sem args) | — | `getledgroups\r\n` |
| resposta | `ledgroups:` + uma entrada por grupo, `;`-terminada | `ledgroups:<name>,<edge>,<width>,<height>,<enabled>,<id1>\|<id2>\|...;` | `ledgroups:bottom,bottom,-1,140,1,4\|5\|6\|7;top,top,-1,90,1,0\|1\|2\|3;\r\n` |
| `setledgroup:` | pedido | `setledgroup:<name>,<edge>,<width>,<height>,<enabled>,<id1>\|<id2>\|...` | `setledgroup:bottom,bottom,-1,140,1,4\|5\|6\|7\r\n` |
| `removeledgroup:` | pedido | `removeledgroup:<name>` | `removeledgroup:bottom\r\n` |
| `applyledgroups` | pedido (sem args) | reaplica todos os grupos habilitados (mesmo caminho do botão em `SettingsWindow`, Fase 5 item 1) | `applyledgroups\r\n` |

Decisões:
- Ordem dos campos espelha exatamente a ordem de declaração do struct `LedGroup` (Fase 2): `name, edge, width, height, enabled, memberIds` — evita uma segunda convenção de ordem para memorizar.
- `edge` como string minúscula (`top`/`bottom`/`left`/`right`/`custom`), mesmo padrão de `fill`/`16:9`/`4:3` em `setcontentaspect`, não o índice numérico do enum (frágil a reordenação do enum).
- `width`/`height` usam `-1` literal para "sem override", igual ao struct — nenhuma tradução extra de valor.
- `memberIds` usa `|` como separador (não `,`, que já separa os campos de nível superior, nem `-`, que aparece em valores negativos de `width`/`height`).
- Nomes de grupo não podem conter `,`, `;` ou `|` — validado na entrada de `setledgroup:`, rejeitado com `CmdSetResult_Error` (mesmo resultado usado por `setcontentaspect` para presets inválidos); não é uma limitação nova, apenas evita ambiguidade de parsing.
- `setledgroup:` faz upsert (cria se o nome não existir, substitui por completo se existir) — não há um comando de "editar só um campo", mesma filosofia de "documento completo" já usada por `setledgroups`/`LayoutRecipe`.
- Todos os comandos de escrita (`setledgroup:`, `removeledgroup:`, `applyledgroups`) exigem client locked, retornam `CmdSetResult_Ok`/`CmdSetResult_Error`/`CmdSetResult_NotLocked`/`CmdSetResult_Busy`, exatamente como `setcontentaspect`.

## 3. Testes

- [x] Teste unitário de `resizedKeepingAnchor`: cada uma das 4 âncoras (`Qt::TopLeftCorner`, etc.) mantém o canto oposto fixo corretamente; `newWidth<=0`/`newHeight<=0` preserva o eixo correspondente; resultado nunca tem largura/altura menor que 1.
- [x] Teste unitário de serialização de `LedGroup` (JSON round-trip), mesmo padrão de `LayoutRecipeGeneratorTest::testJsonRoundTrip` (`Software/tests/LayoutRecipeGeneratorTest.cpp`).
- [x] Teste de `LedGroupRuntime::applyGroup` operando direto sobre `Settings::setLedPosition/setLedSize` (sem `GrabWidget` real — evita depender de `QApplication`/janelas reais, mesmo limite já aceito para `GrabManager` em `docs/plans/smoothing-host-side.md`): grupo com `edge=Top` só altera `height` dos membros, preserva `width`/posição horizontal; `edge=Custom` altera ambos.
- [x] Teste de `memberIds` inválidos (fora do intervalo de LEDs do perfil): aplicação não falha, ignora os IDs inválidos, aplica normalmente aos válidos.
- [x] Teste de grupos sobrepostos: dois grupos habilitados com um LED em comum — o resultado final reflete o último grupo aplicado (ordem de criação), não uma mistura.
- [x] Teste de regressão: perfil sem nenhum `LedGroup` definido (lista vazia/ausente) é bit-a-bit equivalente ao comportamento atual — nenhuma chamada nova a `setLedPosition`/`setLedSize` ocorre sem grupos configurados.

## 4. Critérios de aceite

- Redimensionar todas as caixas do monitor atual de uma vez funciona dentro do wizard, sem precisar arrastar caixa por caixa.
- Criar um grupo nomeado (ex.: `bottom` com altura maior para um pedestal) aplica o override só aos membros daquele grupo, sem mexer nos demais LEDs nem em IDs/enabled/coeficientes.
- Editar uma caixa individualmente arrastando o mouse continua funcionando exatamente como hoje, mesmo depois de usar resize global ou grupos (regressão zero no "modo avançado").
- Um perfil sem nenhum grupo definido é indistinguível do comportamento pré-plano.
- Reaplicar um grupo depois de mudanças manuais nos LEDs membros substitui essas mudanças (comportamento avisado na UI, não escondido).
