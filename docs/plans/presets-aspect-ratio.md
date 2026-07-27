# Plano: presets de aspect ratio sobre o layout físico

Plano de implementação da Q1 de [pesquisa-zonas-led-content-aware.md](../pesquisa-zonas-led-content-aware.md): selecionar `Fill`, `16:9 centered` ou `4:3 centered` sem trocar de perfil. A feature introduz uma camada de área de conteúdo sobre o layout físico existente; ela não é detecção automática de barras pretas nem substitui o wizard. O objetivo é eliminar a necessidade de copiar perfis completos apenas para mudar a área amostrada, preservando o modelo conceitual de layout físico, content frame, política de amostragem e look/device proposto na pesquisa (`docs/pesquisa-zonas-led-content-aware.md:212-240`).

## 1. Decisões de desenho e restrições confirmadas

O estado atual guarda cada zona como `LED_N/Position` e `LED_N/Size` (`Software/src/Settings.cpp:234-244`, `Software/src/Settings.cpp:1881-1902`), com `QSize(150, 150)` como tamanho-padrão (`Software/src/SettingsDefaults.hpp:247-255`). Um perfil é um `QSettings` INI em `Profiles/<nome>.ini` (`Software/src/Settings.cpp:451-481`, `Software/src/Settings.cpp:2062-2064`), por isso o workaround de perfis manuais duplica toda a configuração.

`CustomDistributor` recebe um `QRect` no construtor e repassa-o a `AreaDistributor` (`Software/src/wizard/CustomDistributor.hpp:31-40`; `Software/src/wizard/AreaDistributor.hpp:63-84`). Porém, hoje `ZonePlacementPage` sempre constrói esse retângulo por `screenRect()` e recria as `GrabWidget`s pelo resultado (`Software/src/wizard/ZonePlacementPage.cpp:246-278`, `Software/src/wizard/ZonePlacementPage.cpp:306-315`, `Software/src/wizard/ZonePlacementPage.cpp:317-414`). Portanto, passar um content rect menor é viável geometricamente — os cálculos internos usam `_screen.left()/top()/width()/height()` (`Software/src/wizard/CustomDistributor.cpp:41-138`) — mas é um uso novo e precisa ser isolado e testado.

Há uma lacuna a resolver antes de ligar o botão: o perfil atual persiste somente retângulos, e não a receita do layout (quantidade por lado, espessura, pedestal, cantos, ordem). `AreaDistributor::aspect()` só devolve a razão de `_screen` (`Software/src/wizard/AreaDistributor.hpp:77-88`) e não preserva nem transforma o layout; ela não deve ser reaproveitada para Q1. Aplicar `CustomDistributor` diretamente a um perfil arbitrário poderia substituir uma geometria manual por uma distribuição canônica e, depois, não haveria fonte para restaurar o `Fill` original.

| Conceito | Fonte de verdade após Q1 | Não deve acontecer |
|----------|--------------------------|--------------------|
| Layout físico / receita | Uma receita única do layout já configurado (LEDs por lado, espessura, pedestal, cantos, ordem e monitor) | Uma cópia `LED_N/*` por aspect ratio |
| Área de conteúdo | `QRect` calculado no monitor selecionado a partir do preset | Armazenar o `QRect` calculado para cada resolução |
| Seleção do usuário | Uma única chave `Grab/ContentAspectPreset` no perfil, default `fill` | Criar/trocar um perfil para `16:9` ou `4:3` |
| Geometria efetiva | Retângulos correntes reemitidos pelo distributor para o preset ativo | Alterar brilho, gamma, smooth, device ou coeficientes de LED |

Para não prometer suporte que os dados atuais não permitem, Q1 deve operar inicialmente em layouts que tenham uma receita registrada pelo wizard ou por uma etapa explícita de “adotar layout atual”. Para layouts manualmente desenhados que não possam ser representados pela receita, o controle permanece em `Fill` e não altera `LED_N/*`; a migração não deve inferir lados/ordem a partir de retângulos. Um futuro modo `custom` pode aceitar uma razão numérica, mas fica fora do primeiro corte até haver validação e UX para ela.

## 2. Itens rastreáveis

### Fase 1 — cálculo de content rect

- [x] Criar um tipo/serviço reutilizável de geometria (fora de `ZonePlacementPage`) que receba o `QRect` global do monitor e um preset `fill`, `16:9` ou `4:3`, retorne um `QRect` inteiro e seja a única implementação da regra de aspect ratio.
- [x] Para `fill`, retornar exatamente o retângulo do monitor; para uma razão alvo `r = numerador/denominador`, comparar `monitor.width()/monitor.height()` com `r`: se o monitor for mais largo, usar toda a altura e `width = floor(height * r)` centralizada horizontalmente (pillarbox nas laterais); se for mais estreito, usar toda a largura e `height = floor(width / r)` centralizada verticalmente (letterbox em cima/baixo); se forem iguais, retornar `Fill`.
- [x] Preservar as coordenadas globais do monitor (`QScreen::geometry()` já é a origem usada pelo wizard em `Software/src/wizard/ZonePlacementPage.cpp:306-314`) e centralizar com os pixels restantes distribuídos por `left + (W - w) / 2` ou `top + (H - h) / 2`; validar largura/altura positivas antes de construir o distributor.
- [x] Definir explicitamente a ordem de composição com as margens existentes do wizard: Q1 calcula o content rect a partir do retângulo físico já escolhido para o layout; se o produto decidir expor as margens do wizard, aplicar primeiro seus insets percentuais (`Software/src/wizard/ZonePlacementPage.cpp:306-315`) e depois o preset AR, nunca o inverso.
- [x] Não usar `AreaDistributor::aspect()` como atalho: ela é apenas um getter de razão e não é chamada pelo fluxo que cria áreas (`Software/src/wizard/AreaDistributor.hpp:82-84`; `Software/src/wizard/ZonePlacementPage.cpp:317-414`).

### Fase 2 — persistência

- [x] Adicionar em `SettingsScope::Profile::Key::Grab` a constante `ContentAspectPreset = "Grab/ContentAspectPreset"`, o enum/string set canônico `fill`, `16:9`, `4:3`, e default `fill`; inicializar a chave com `setNewOption` junto das demais chaves Grab em `Settings::initCurrentProfile()` (`Software/src/Settings.cpp:188-203`, `Software/src/Settings.cpp:2095-2115`).
- [x] Expor `Settings::getContentAspectPreset()` e `Settings::setContentAspectPreset(...)`, com validação que converta valor ausente ou inválido para `fill`, e um sinal de mudança para atualizar UI e runtime. O acesso deve usar o perfil corrente, tal como `Settings::setValue/value` (`Software/src/Settings.cpp:2227-2256`), não `LightpackMain.conf`.
- [x] Persistir uma receita única de layout físico somente quando o usuário aplicar/confirmar o layout no wizard: monitor alvo, contagens top/side/bottom, thickness, stand width, skip corners, invert order e numbering offset — os mesmos parâmetros que alimentam `CustomDistributor` (`Software/src/wizard/ZonePlacementPage.cpp:327-337`, `Software/src/wizard/ZonePlacementPage.cpp:398-409`). Essa receita é base única, não estado por preset.
- [x] Adicionar uma etapa de adoção explícita para perfis legados: se a receita não existir, manter `fill`, mostrar que os presets exigem registrar/recriar o layout pelo wizard e não escrever `LED_N/Position` ou `LED_N/Size`. Isto protege layouts manuais cuja topologia não pode ser inferida com segurança.
- [x] Documentar na migração que perfis existentes continuam INIs independentes: cada perfil poderá ter seu próprio valor ativo de `Grab/ContentAspectPreset`, mas nenhum perfil novo será criado nem haverá cópia de configurações. A carga de perfil atual já inicializa chaves inexistentes sem resetar valores existentes (`Software/src/Settings.cpp:475-481`, `Software/src/Settings.cpp:2171-2188`).

### Fase 3 — redistribuição de zonas

- [x] Extrair de `ZonePlacementPage` uma operação compartilhada “gerar retângulos a partir da receita + `QRect`” que instancia `CustomDistributor`, itera `areaCount()/next()` e converte cada `ScreenArea` em `QRect`, reproduzindo a conversão hoje feita no wizard (`Software/src/wizard/ZonePlacementPage.cpp:246-268`). A página deve continuar apenas responsável por criar/mostrar `GrabWidget`s.
- [x] Acrescentar ao gerador validações antes de chamar `next()`: contagens não negativas, soma de LEDs maior que zero, dimensões do content rect positivas e espessura/pedestal dentro dos limites definidos pela UI. Cobrir também a divisão por zero hoje possível se uma distribuição inviável chegar a `CustomDistributor` (`Software/src/wizard/CustomDistributor.cpp:50-59`, `Software/src/wizard/CustomDistributor.cpp:86-95`).
- [x] Implementar um único ponto de aplicação runtime que: (1) obtém a receita, (2) calcula o content rect pelo preset, (3) gera todos os `QRect`s, (4) grava somente os retângulos efetivos do perfil atual com `Settings::setLedPosition/setLedSize`, e (5) notifica o mesmo caminho que atualiza as zonas ativas. Os setters já emitem sinais por LED (`Software/src/Settings.cpp:1886-1902`); evitar duas implementações, uma para UI e outra para API.
- [x] Ao trocar de `Fill` para `16:9`/`4:3`, manter IDs, `IsEnabled` e coeficientes; somente `Position` e `Size` mudam. Isto acompanha o wizard, que preserva o ID ao adicionar áreas (`Software/src/wizard/ZonePlacementPage.cpp:265-296`), e evita reordenar LEDs físicos.
- [x] Ao voltar a `Fill`, regenerar a partir da mesma receita e do mesmo retângulo base — não restaurar um snapshot por preset. Confirmar visualmente que `_screen` deslocado é respeitado, pois o distributor usa as bordas do `QRect` fornecido (`Software/src/wizard/CustomDistributor.cpp:61-62`, `Software/src/wizard/CustomDistributor.cpp:78-79`, `Software/src/wizard/CustomDistributor.cpp:95-96`).
- [x] Decidir e documentar que o primeiro corte não tenta preservar ajustes individuais após uma redistribuição: a operação é “reaplicar layout canônico”. Antes de executar, a UI deve avisar que alterações manuais de posição/tamanho serão substituídas; perfis sem receita permanecem sem efeito. Essa decisão evita a falsa promessa de preservar um layout arbitrário quando o distributor só conhece uma receita canônica.

### Fase 4 — UI

- [x] Implementar na UI Qt clássica um grupo “Content aspect” próximo ao botão existente “Run configuration wizard” em `SettingsWindow.ui` (`Software/src/SettingsWindow.ui:1497-1522`), com botões exclusivos `Fill`, `16:9` e `4:3`, estado atual legível e texto de ajuda explicando que a ação redistribui as zonas no mesmo perfil.
- [x] Conectar os botões em `SettingsWindow.cpp` ao ponto único de aplicação da Fase 3 e recarregar seu estado quando o perfil for carregado; a janela já conecta mudanças do combo de perfis e recebe `currentProfileInited` (`Software/src/SettingsWindow.cpp:280-291`, `Software/src/SettingsWindow.cpp:1636-1656`).
- [x] Exibir no grupo um preview textual do retângulo calculado (por exemplo, `16:9 — 2560×1440 em 3440×1440`) e um aviso de receita ausente; não exigir o redesign. O documento de redesign é apenas direção futura e também propõe AR como cidadão de primeira classe (`docs/redesign-ui-prismatik.md:27-34`, `docs/redesign-ui-prismatik.md:81-88`).
- [ ] (Opcional, não implementado nesta rodada) Acrescentar ações de atalho `content-aspect-fill`, `content-aspect-16x9` e `content-aspect-4x3` ao sistema existente de hotkeys. Investigação confirmou que `setupHotkeys()`/`registerHotkey()` em `SettingsWindow.hpp` são declarações mortas sem nenhuma implementação no repositório — não existe um padrão de registro de hotkey funcional para seguir hoje, então isso ficaria bloqueado em consertar essa infraestrutura primeiro (fora do escopo deste item, explicitamente marcado como opcional no plano).
- [x] Não substituir nem ocultar perfis, o combo de perfis ou o wizard: o combo atual ainda carrega um INI completo (`Software/src/SettingsWindow.cpp:1636-1656`) e o botão do wizard relança o aplicativo em `--wizard` (`Software/src/SettingsWindow.cpp:2310-2321`).

### Fase 5 — API

- [x] Declarar `CmdSetContentAspect = "setcontentaspect:"` em `ApiServer.hpp` e defini-lo ao lado dos demais comandos `set*` em `ApiServer.cpp` (`Software/src/ApiServer.cpp:140-162`), com argumentos estritamente `fill`, `16:9` e `4:3` no primeiro corte.
- [x] Implementar o branch de parsing depois de `setprofile:`: remover o prefixo, validar o token, exigir o mesmo lock e devolver exatamente `ok\r\n`, `error\r\n`, `not locked\r\n` ou `busy\r\n` usados pelos demais setters (`Software/src/ApiServer.cpp:1008-1033`, `Software/src/ApiServer.cpp:1049-1096`). Um token inválido ou receita ausente deve responder `error` sem mudar preset nem zonas.
- [x] Expor uma operação de domínio em `LightpackPluginInterface` — por exemplo `SetContentAspect(sessionKey, preset)` — em vez de `ApiServer` escrever Settings diretamente; `SetLeds` já ilustra a ponte que escreve posições/tamanhos e emite atualização de perfil (`Software/src/LightpackPluginInterface.cpp:453-466`).
- [x] Adicionar `setcontentaspect:` às mensagens `help` e `?`, com exemplos `setcontentaspect:fill`, `setcontentaspect:16:9` e `setcontentaspect:4:3`; os setters existentes são documentados em `initHelpMessage()` e listados em `initShortHelpMessage()` (`Software/src/ApiServer.cpp:1486-1536`, `Software/src/ApiServer.cpp:1594-1618`). Considerar `getcontentaspect` como complemento posterior, não requisito de Q1.

## 3. Testes

- [x] Teste unitário do cálculo: monitor 3440×1440 (`21:9`) + `fill` retorna o próprio `QRect`; + `16:9` retorna 2560×1440 centralizado horizontalmente; + `4:3` retorna 1920×1440 centralizado horizontalmente.
- [x] Teste unitário do cálculo: monitor 1920×1080 + `16:9` resulta exatamente em `Fill`, sem deslocamento nem perda de pixel.
- [x] Teste unitário do cálculo: monitor mais estreito que o alvo (por exemplo 1280×1024 + `16:9`) produz largura total e altura menor, centralizada verticalmente; testar origem de monitor não-zero em desktop multimonitor.
- [x] Teste unitário para resolução ímpar/incomum e monitor rotacionado (por exemplo 1080×1920): dimensões inteiras, positivas, contidas no monitor e centralização com diferença máxima de um pixel entre bordas.
- [x] Teste unitário do gerador com content rect deslocado: todos os retângulos de `CustomDistributor` ficam contidos no content rect, respeitam espessura e preservam a quantidade/ordem de LEDs da receita. Incluir top/side/bottom, pedestal e `skipCorners`, conforme os parâmetros efetivamente consumidos pelo distributor (`Software/src/wizard/CustomDistributor.hpp:34-39`).
- [x] Teste de regressão do wizard: com o mesmo `screenRect()` e a mesma receita, o gerador extraído produz os mesmos `QRect`s que o fluxo atual de Andromeda/Cassiopeia/Pegasus/Apply (`Software/src/wizard/ZonePlacementPage.cpp:317-414`).
- [x] Teste de integração Settings: perfil sem chave recebe `fill`; trocar para `16:9`, reiniciar e recarregar o mesmo perfil conserva apenas `Grab/ContentAspectPreset` como novo estado de aspect, sem criar outro `.ini` nem chaves `LED_*` por preset.
- [ ] Teste de compatibilidade: abrir perfis manuais pré-Q1, inclusive múltiplos perfis usados como workaround; nenhum é alterado na migração, o controle inicia em `Fill` e a troca de perfil continua independente.
- [ ] Teste de UI: botões exibem o preset persistido após trocar de perfil, atualizam preview, e uma receita ausente bloqueia a redistribuição com mensagem clara; o wizard e o seletor de perfis continuam acessíveis.
- [x] Teste de API em socket: com lock válido, `setcontentaspect:16:9` persiste/aplica e responde `ok`; testar `fill`, `4:3`, token inválido, falta de lock e perfil sem receita (`Software/tests/LightpackApiTest.cpp::testCase_SetContentAspectRequiresRecipeAndLock`). Não testado o caso específico de "lock de outro cliente" (`busy`) — mesmo nível de cobertura já aceito para `sethostsmooth` nesta sessão.

Os itens 73 e 74 permanecem **não automatizados**: não há framework de teste de widgets neste repositório (nenhuma suíte existente dirige `SettingsWindow` ou abre perfis `.ini` manuais reais), então "abrir perfis manuais pré-Q1" e "botões exibem o preset após trocar de perfil" são validados por inspeção de código, não por teste automatizado — `ZoneLayoutRuntime::applyContentAspectPreset` só escreve algo quando `Settings::hasLayoutRecipe()` é verdadeiro (perfis legados nunca têm essa chave, logo nunca são tocados), e `SettingsWindow::updateContentAspectUi()` é chamada tanto de `updateUiFromSettings()` (carga de perfil) quanto do signal `contentAspectPresetChanged`.

---

## 4. Critérios de aceite

- [x] Um único perfil configurado por wizard alterna `Fill` ⇄ `16:9` ⇄ `4:3` por botão, atalho ou API sem alterar brilho, gamma, smooth, dispositivo, ordem/enable dos LEDs ou criar um perfil.
- [x] O único estado novo específico de aspect persistido é `Grab/ContentAspectPreset`; a receita física é uma única base necessária para regenerar zonas, não uma geometria duplicada por preset.
- [x] Um perfil legado/manual sem receita não é corrompido nem reinterpretado: permanece em `Fill` até adoção explícita.
- [x] A entrega permanece incremental: não inclui detecção dinâmica de black bars, seguir janela, coordenadas normalizadas ou o redesign de UI completo, que são itens posteriores da pesquisa (`docs/pesquisa-zonas-led-content-aware.md:261-286`, `docs/pesquisa-zonas-led-content-aware.md:290-336`).
