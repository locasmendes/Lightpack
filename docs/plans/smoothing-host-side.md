# Plano de implementação — suavização temporal no host

Os devices Adalight, Ardulight e UDP hoje recebem cada cor processada como um salto instantâneo; somente o Lightpack USB nativo suaviza no firmware AVR. Este plano adiciona uma transição linear genérica no host, de 0 a 400 ms, sem alterar firmware, sketches ou protocolos. Ele complementa a análise de [gargalos do sistema moderno](../gargalos-sistema-moderno-2026.md) e de [firmware/hardware datados](../firmware-hardware-datados.md): a suavização deve reduzir cortes visuais nos devices sem suporte nativo, sem acrescentar uma segunda camada de atraso ao Lightpack.

---

## 1. Decisões de design

| Decisão | Recomendação adotada | Justificativa |
|---|---|---|
| Local da suavização | Implementar no `GrabManager`, entre o pós-processamento/diff de `handleGrabbedColors()` e o sinal `updateLedsColors(...)`. | Todos os devices de ambilight já recebem as cores por esse sinal; a solução cobre serial e UDP sem tocar em cada driver ou protocolo. |
| Estados de cor | Manter `m_colorsCurrent` como o **alvo** já pós-processado; adicionar `m_colorsDisplayed` como o último frame efetivamente emitido e `m_colorsTransitionStart` como a origem da transição. | Separa a cor capturada desejada da cor que o LED está exibindo e evita que a interpolação corrompa o diff de captura atual. |
| Relógio | Usar um `QTimer` dedicado, `Qt::PreciseTimer`, a 16 ms (~62,5 Hz), independente do timer de grab. Usar `QElapsedTimer` monotônico para calcular a fração temporal real. | A transição continua entre dois grabs e não depende do intervalo configurado em `Grab/Slowdown`; `QElapsedTimer` elimina erro acumulado quando o tick atrasa. |
| Fórmula | Para cada canal RGB, emitir `round(start + (target - start) * t)`, com `t = clamp(elapsedMs / durationMs, 0, 1)`. | Define uma interpolação linear reprodutível, inclusive para testes com valores exatos. |
| Re-alvo | Ao chegar um novo alvo durante uma transição, calcular primeiro a cor interpolada no instante atual, copiá-la para `m_colorsTransitionStart`, manter essa cor em `m_colorsDisplayed`, trocar `m_colorsCurrent` pelo novo alvo e reiniciar o cronômetro para a duração integral. | Reiniciar da cor atualmente visível preserva continuidade (sem salto) e responde ao conteúdo novo; reiniciar do alvo/origem anterior criaria uma descontinuidade perceptível. |
| Duração zero | `0 ms` interrompe o timer, copia alvo para exibida e emite imediatamente pelo mesmo caminho de hoje. | Garante compatibilidade comportamental com a instalação atual e com quem não quer smoothing. |
| Lightpack nativo | Não aplicar smoothing no host quando `Settings::getConnectedDevice()` for `SupportedDevices::DeviceTypeLightpack`; manter o controle existente `Device/Smooth` como único smoothing desse device. A UI do novo controle deve ficar oculta/desabilitada com explicação; a guarda no `GrabManager` continua obrigatória para API/headless. | Duas interpolações em série aumentariam o atraso e deixariam o comportamento difícil de prever. O firmware já possui `LedDeviceLightpack::setSmoothSlowdown()` (`Software/src/LedDeviceLightpack.cpp:200`) e é o dono da transição nesse hardware. |
| Default | `150 ms`, com intervalo inteiro contínuo de `0` a `400 ms`, passo fino de `1 ms` no spinbox e page step de `25 ms` no slider. | 150 ms é uma micro-transição útil em capturas padrão de 50 ms sem reproduzir o atraso excessivo do smooth AVR; 0 continua disponível como “Off”. |

### 1.1 Fluxo proposto

```text
m_colorsNew
  -> pós-processamento já existente (temperatura/night light, média, overbrighten)
  -> m_colorsCurrent (alvo mais recente)
  -> [smoothing host, quando ativo e device != Lightpack]
       m_colorsTransitionStart + QElapsedTimer -> m_colorsDisplayed
  -> emit updateLedsColors(m_colorsDisplayed)
  -> LedDeviceManager -> driver serial/UDP
```

`GrabManager::handleGrabbedColors()` já é o ponto correto: ele copia `m_colorsNew` para `m_colorsProcessing`, aplica o pós-processamento, atualiza `m_colorsCurrent` quando há diff e hoje emite esse array diretamente (`Software/src/GrabManager.cpp:430–493`). O timer de fake grab (`timeoutFakeGrab()`, `Software/src/GrabManager.cpp:504–514`) não deve competir com o timer de transição: durante uma transição ativa, só o timer de smoothing emite frames intermediários; quando não houver transição, o comportamento existente de resend/fake grab permanece.

### 1.2 Contrato do motor

Adicionar ao `GrabManager` uma API interna com nomes próximos aos seguintes (a implementação pode ajustar detalhes de assinatura, não a semântica):

```cpp
void onGrabHostSmoothingDurationChanged(int ms);
void setDisplayedColorsImmediately(const QList<QRgb>& colors);
void startOrRetargetHostTransition();
void advanceHostTransition(); // slot do QTimer dedicado
bool isHostSmoothingApplicable() const;
```

- `m_colorsCurrent` continua a receber o resultado final do processamento por LED, inclusive valores de LEDs desabilitados tal como hoje; ele não é reescrito pelo timer.
- Antes de re-alvo, `advanceHostTransition()` deve ser executado uma vez com o tempo atual para materializar `m_colorsDisplayed`. O novo par `(start, target)` é, portanto, `(displayedAtual, m_colorsCurrentNovo)`.
- Se o novo alvo for idêntico ao alvo vigente, não reiniciar a duração. Se a cor exibida já alcançar o alvo, emitir o frame final exatamente uma vez e parar o timer.
- Para `0 ms`, device Lightpack, listas ainda não inicializadas ou mudança de quantidade de LEDs, cancelar a transição e sincronizar início/exibida/alvo para um estado consistente. `initColorLists()`, `clearColorsCurrent()` e `reset()` precisam tratar também as novas listas e o timer, para que resize/reset não misturem arrays de tamanhos diferentes.
- Ao parar o ambilight ou desligar LEDs, cancelar a transição e enviar preto imediatamente pelo fluxo já utilizado para desligamento; não deixar uma transição pendente manter LEDs acesos.
- O contador de FPS de capture continua em `handleGrabbedColors()`; se for desejada telemetria de frames enviados, criar métrica distinta em vez de alterar o significado de `ambilightTimeOfUpdatingColors` (`Software/src/GrabManager.cpp:516–522`).

---

## 2. Itens rastreáveis

### Fase 1 — Settings/persistência

- [x] Em `Software/src/SettingsDefaults.hpp`, adicionar a `SettingsScope::Profile::Grab` as constantes `HostSmoothingDurationMin = 0`, `HostSmoothingDurationDefault = 150` e `HostSmoothingDurationMax = 400`; mantê-las fora de `Profile::Device`, pois a funcionalidade é do pipeline host e não um comando de firmware.
- [x] Em `Software/src/Settings.cpp`, declarar a chave de perfil `Profile::Key::Grab::HostSmoothingDuration` como `"Grab/HostSmoothingDuration"`, registrar seu valor em `setNewOption(...)` junto às demais opções Grab e implementar getter, setter e clamp seguindo o padrão de `getDeviceSmooth()`/`getValidDeviceSmooth()` (`Software/src/Settings.cpp:1465–1475, 1949–1956`).
- [x] Em `Software/src/Settings.hpp`, expor `getGrabHostSmoothingDuration()`, `setGrabHostSmoothingDuration(int)` e o signal `grabHostSmoothingDurationChanged(int)`; emitir o valor validado, não o argumento bruto.
- [x] Carregar essa opção em `GrabManager::settingsProfileChanged()` e conectá-la em `LightpackApplication::startLedDeviceManager()` diretamente ao novo slot do `GrabManager`, ao lado das demais configurações de grab. Confirmar que a troca de perfil reconfigura a duração e cancela/reinicia corretamente uma transição em curso.

### Fase 2 — motor de interpolação

- [x] Em `Software/src/GrabManager.hpp`, adicionar `QTimer* m_timerHostSmoothing`, `QElapsedTimer m_hostSmoothingElapsed`, duração em ms, `m_colorsDisplayed`, `m_colorsTransitionStart` e os slots/helpers do contrato da seção 1.2; incluir os headers Qt necessários.
- [x] Em `Software/src/GrabManager.cpp`, construir o timer dedicado com parent `this`, `Qt::PreciseTimer`, intervalo de 16 ms e conexão para `advanceHostTransition()`; ele não deve alterar `m_grabber` nem o intervalo de `Grab/Slowdown`.
- [x] Alterar `handleGrabbedColors()` para conservar o pós-processamento e o diff existentes (`Software/src/GrabManager.cpp:430–489`), mas, quando o alvo mudar, iniciar/re-alvejar o motor em vez de sempre emitir `m_colorsCurrent`; no caminho inativo, emitir imediatamente `m_colorsDisplayed` após sincronizá-lo ao alvo, preservando o comportamento atual de `m_isSendDataOnlyIfColorsChanged`.
- [x] Implementar interpolação por canal com arredondamento para o inteiro mais próximo e clamp `[0, 255]`; emitir somente se `m_colorsDisplayed` mudou desde o último frame enviado, exceto no resend deliberado exigido por `m_isSendDataOnlyIfColorsChanged == false` fora de transição.
- [x] Fazer `timeoutFakeGrab()` não emitir frames duplicados enquanto `m_timerHostSmoothing` estiver ativo; deixar o timer dedicado ser a única fonte dos intermediários e retomar o fake grab normal após a conclusão.
- [x] Estender `initColorLists()`, `clearColorsCurrent()` e `reset()` (`Software/src/GrabManager.cpp:716–747`) para inicializar/limpar os três arrays de estado e parar o timer de maneira atômica no thread do `GrabManager`.
- [x] Documentar em comentário curto junto ao estado que `m_colorsCurrent` é o alvo processado e `m_colorsDisplayed` é a última cor emitida, para evitar regressão futura que use a lista exibida no diff de captura.

### Fase 3 — UI

- [x] Adicionar em `Software/src/SettingsWindow.ui`, na área de opções de Grab (não no grupo de parâmetros do firmware), uma linha “Host smoothing” com slider horizontal `0..400`, `singleStep=1`, `pageStep=25`, spinbox em milissegundos e rótulo dinâmico “Off” para 0 e “N ms” nos demais valores.
- [x] Explicar no `whatsThis` que a duração é uma transição linear do último frame exibido para a cor capturada mais recente, que 0 preserva o corte atual e que o recurso atende devices sem smoothing próprio.
- [x] Em `Software/src/SettingsWindow.hpp/.cpp`, adicionar o slot de mudança, preencher os widgets em `updateUiFromSettings()` (que já alimenta os controles de device em `Software/src/SettingsWindow.cpp:1950–1963`) e persisti-lo por `Settings::setGrabHostSmoothingDuration()`.
- [x] Reusar a atualização de visibilidade por device já centralizada em `SettingsWindow::setDeviceTabWidgetsVisibility(...)` (`Software/src/SettingsWindow.cpp:466`) ou criar helper equivalente para a seção Grab: para `DeviceTypeLightpack`, ocultar/desabilitar a linha nova e mostrar texto “O Lightpack usa Device Smooth no firmware”; para os demais devices, exibir e habilitar a linha.
- [x] Não renomear nem reaproveitar `horizontalSlider_DeviceSmooth`/`spinBox_DeviceSmooth`: eles controlam `Device/Smooth` (`Software/src/SettingsWindow.cpp:1356–1361`) e continuam exclusivos do Lightpack. Isto preserva perfis existentes e evita confundir “steps do firmware” com milissegundos do host.

### Fase 4 — integração com device Lightpack

- [x] Implementar `isHostSmoothingApplicable()` com uma guarda baseada em `Settings::getConnectedDevice() != SupportedDevices::DeviceTypeLightpack`, além de `duration > 0`; aplicá-la no ponto de emissão, não apenas na UI.
- [x] Quando o device mudar para Lightpack, parar a transição host, sincronizar as listas ao alvo e fazer os próximos frames seguirem diretamente para o firmware; quando mudar para outro device, iniciar sem herdar uma transição parcial do device anterior.
- [x] Manter `Device/Smooth` e a cadeia `Settings::deviceSmoothChanged -> LedDeviceManager::setSmoothSlowdown` inalteradas (`Software/src/LightpackApplication.cpp:699–703`); os stubs de Adalight/Ardulight (`Software/src/LedDeviceAdalight.cpp:172–175`) não devem receber lógica nova.
- [x] Registrar no texto de ajuda e na documentação de API que “host smoothing” é ignorado no Lightpack nativo para prevenir dupla suavização, mesmo em modo headless ou quando o valor foi persistido por outro device.

### Fase 5 — API

- [x] Adicionar comandos independentes `gethostsmooth` e `sethostsmooth:<0..400>` em `Software/src/ApiServer.hpp/.cpp`, com resposta `hostsmooth:<ms>`, validação de até três dígitos e entrada de help, seguindo a estrutura de `getsmooth`/`setsmooth:` (`Software/src/ApiServer.cpp:114–115, 151, 597–602, 828–865`).
- [x] Acrescentar cache, getter/setter e signals distintos em `Software/src/LightpackPluginInterface.hpp/.cpp` (por exemplo, `GetHostSmooth()` e `SetHostSmooth(...)`) e conectar o signal ao novo setter de Settings/GrabManager; não reutilizar `SetSmooth`, que representa o protocolo legado do firmware e tem faixa `0..255`.
- [x] Exigir o mesmo lock de sessão dos demais comandos mutáveis. Em Lightpack, aceitar e persistir o valor para permitir perfis multi-device, mas documentar e retornar normalmente que ele está inativo; a guarda de execução continua no `GrabManager`.
- [x] Atualizar os testes de API existentes em `Software/tests/LightpackApiTest.cpp` para o get/set, limites 0/400, rejeição de 401 e requisitos de lock, preservando os testes de `setsmooth` como contrato do firmware.

---

## 3. Arquivos prováveis a tocar

| Arquivo | Motivo |
|---|---|
| `Software/src/GrabManager.hpp` | Declarar timer, relógio, arrays de origem/exibida, duração e slots/helpers do motor. |
| `Software/src/GrabManager.cpp` | Retarget, interpolação, emissão e limpeza de estado no pipeline central. |
| `Software/src/SettingsDefaults.hpp` | Faixa e default perfilados do novo valor host. |
| `Software/src/Settings.hpp` | API pública de Settings e signal de alteração. |
| `Software/src/Settings.cpp` | Chave `Grab/HostSmoothingDuration`, validação e persistência por perfil. |
| `Software/src/LightpackApplication.cpp` | Conectar a alteração de Settings ao `GrabManager`, sem passar pelo `LedDeviceManager`. |
| `Software/src/SettingsWindow.ui` | Slider/spinbox/ajuda da nova duração na seção Grab. |
| `Software/src/SettingsWindow.hpp` e `Software/src/SettingsWindow.cpp` | Slots, carga de UI e visibilidade específica para Lightpack. |
| `Software/src/LightpackPluginInterface.hpp` e `Software/src/LightpackPluginInterface.cpp` | Estado e sinais próprios para a API host-side. |
| `Software/src/ApiServer.hpp` e `Software/src/ApiServer.cpp` | Parser, respostas e ajuda de `gethostsmooth`/`sethostsmooth`. |
| `Software/tests/LightpackApiTest.cpp` | Cobertura do contrato API e dos limites persistidos. |
| Novo teste unitário Qt para o motor, ou extensão da configuração de testes em `Software/tests/tests.pro` | Tornar determinísticos cálculo, retarget e bypass Lightpack sem depender de hardware/timer real. |

`LedDeviceAdalight.cpp`, `LedDeviceArdulight.cpp`, `AbstractLedDeviceUdp.*`, sketches e `Firmware/` são deliberadamente **não** alterados: esse é um recurso comum do host e os protocolos continuam recebendo apenas frames RGB completos.

---

## 4. Testes

- [x] Testar `duration=0`: para um alvo alterado, emitir imediatamente exatamente `m_colorsCurrent`, não iniciar `m_timerHostSmoothing` e manter o comportamento de diff/resend idêntico ao de `GrabManager::handleGrabbedColors()` atual.
- [x] Testar matemática linear com valores conhecidos: de `(0, 100, 200)` para `(100, 0, 0)` em 200 ms, verificar em 0/100/200 ms respectivamente `(0,100,200)`, `(50,50,100)` e `(100,0,0)`, incluindo arredondamento de frações como 0,5.
- [x] Testar re-alvo no meio da transição: de preto para vermelho 200 em 200 ms, avançar 100 ms (vermelho 100), re-alvejar para azul 200 e verificar que o primeiro frame da nova transição parte de `(100,0,0)`, sem retorno a preto nem salto para o alvo anterior; confirmar a chegada ao azul após outros 200 ms.
- [x] Testar que alvo idêntico não reinicia o relógio, que a transição para no frame final e que não há emissão duplicada do fake grab durante timer ativo.
- [x] Testar resize/troca de perfil/reset com número de LEDs diferente: arrays de alvo, início e exibida ficam com o mesmo tamanho, e nenhum tick posterior acessa índice inválido.
- [x] Testar mudança de duração durante transição: mudar para 0 conclui imediatamente no alvo; mudar de um valor positivo para outro re-alveja da cor exibida atual com a nova duração, sem descontinuidade.
- [ ] Testar device Lightpack: com `Grab/HostSmoothingDuration=150`, `isHostSmoothingApplicable()` é falso, o host emite alvo direto e não cria frames intermediários; o controle `Device/Smooth` continua sendo enviado por `LedDeviceManager` ao firmware, provando que não há dupla suavização.
- [ ] Testar Adalight, Ardulight e pelo menos um driver UDP com duração positiva usando um fake/spy de `updateLedsColors`: observar frames intermediários sem qualquer chamada nova a `setSmoothSlowdown` dos devices.
- [x] Testar API: `gethostsmooth` devolve o valor do perfil; `sethostsmooth:0` e `sethostsmooth:400` são aceitos com lock; `401`, texto não numérico e chamadas sem lock são rejeitados; no Lightpack o valor é persistido, mas o pipeline permanece bypassado.

Os itens 127-128 permanecem **não automatizados**: exercitá-los exigiria instanciar `GrabManager` de verdade, que constrói grabbers de tela reais (Desktop Duplication/GDI/D3D10) e widgets — inviável no binário de testes atual (nenhuma suíte existente instancia `GrabManager`). O motor de interpolação (`HostColorSmoothing`) que `GrabManager` usa internamente está coberto exaustivamente por `Software/tests/HostColorSmoothingTest.cpp` (itens 121-126), e a guarda `isHostSmoothingApplicable()` em si é uma linha (`Software/src/GrabManager.cpp`) verificada por leitura de código, não por teste automatizado.

## 5. Critério de aceite

O recurso está pronto quando um device não-Lightpack pode escolher Off ou qualquer duração de 1–400 ms, cada alvo pós-processado transiciona linearmente a partir da cor que estava efetivamente exibida, e o Lightpack nativo continua usando exclusivamente seu smoothing AVR. Com `0 ms`, os frames, os diffs e a cadência observável do caminho normal são equivalentes ao comportamento pré-mudança.
