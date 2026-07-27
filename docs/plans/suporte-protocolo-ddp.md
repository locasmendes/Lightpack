# Plano de implementação — suporte ao protocolo DDP

Este plano adiciona DDP (Distributed Display Protocol) como mais um device UDP do Prismatik, seguindo a arquitetura de WARLS, DRGB e DNRGB. Ele complementa o contexto de transporte já registrado em [`firmware-hardware-datados.md`](../firmware-hardware-datados.md#33-transporte-usb-hid-full-speed-vs-alternativas-mais-rápidas), que confirma que DDP ainda não é implementado no repositório (`docs/firmware-hardware-datados.md:149-155`).

## 1. Premissas do protocolo — confirmado

DDP usa UDP na porta padrão `4048`. Cada datagrama tem cabeçalho de 10 bytes (14 se o flag de timecode estiver setado, não usado por esta implementação) e payload RGB de no máximo 480 pixels (1440 bytes). Layout binário confirmado por duas fontes independentes: o código real de recepção do WLED (`wled00/e131.cpp`, `wled00/src/dependencies/e131/ESPAsyncE131.h`) e o sender de referência testado `ddp-rs` (`coral/ddp-rs`, `src/protocol/frame.rs`, `src/protocol/pixel_config.rs`, `src/packet.rs`).

| Byte(s) | Campo | Valor/regra confirmada |
|---|---|---|
| 0 | `flags` | Bits `VV T S R Q P`. `VV=01` (versão 1) → bit 6 setado (`0x40`). `P` (push, bit 0, `0x01`) marca "renderizar agora": **false em todos os pacotes intermediários de um frame segmentado, true apenas no último pacote** (inclusive quando o frame cabe em 1 pacote). `T`/`S`/`R`/`Q` não são usados por este sender (ficam em 0). |
| 1 | `sequence` | 4 bits baixos, contador rolante `1..15` que envolve para `1` (nunca усa 0, que na spec significa "sequência desabilitada"); incrementa a cada pacote enviado (entre frames também, não reinicia por frame), mantido como estado por instância de `LedDeviceDdp`. |
| 2 | `data type` | `0x0B` (`DDP_TYPE_RGB24` — nome usado pelo WLED). Formato do byte é `C R TTT SSS`; `0x0B` e o `0x0D` usado por padrão no `ddp-rs` têm o mesmo `TTT=001` (RGB) mas `SSS` diferente (3 vs. 5) — **não são o mesmo valor**, e cada projeto rotula esse subcampo de profundidade de forma diferente (WLED chama `SSS=3` de "24-bit" mesmo não seguindo a mesma convenção "bits totais" do `ddp-rs`). São intercambiáveis apenas *na prática para o parser do WLED*, porque a validação do byte inteiro contra `DDP_TYPE_RGB24`/`RGBW32` está comentada em `e131.cpp` — o código só lê os bits `TTT` (via `(dataType & 0b00111000) >> 3`) para decidir 3 vs. 4 canais e ignora `SSS`. Um receiver que validasse o byte inteiro contra uma constante fixa aceitaria só um dos dois; ficamos com `0x0B` por ser o nome/constante que o WLED de fato documenta e expõe. |
| 3 | `ID`/destino | `0x01` (`DDP_ID_DISPLAY`, saída padrão). |
| 4–7 | `offset` | 32 bits, **big-endian** (network byte order). Offset em **bytes** dentro do frame (não índice de pixel); reinicia em `0` a cada novo frame processado e avança pelo tamanho do payload de cada pacote anterior dentro do mesmo frame. |
| 8–9 | `length` | 16 bits, **big-endian**. Tamanho do payload deste pacote em bytes (até 1440; o último pacote de um frame carrega o resto, que pode ser menor). |
| 10+ | payload | `r,g,b` sequenciais, sem padding, na ordem de entrada. |

Fontes literais consultadas: struct de cabeçalho e constantes em `ESPAsyncE131.h` (`DDP_FLAGS_VER1=0x40`, `DDP_FLAGS_PUSH=0x01`, `DDP_TYPE_RGB24=0x0B`, `DDP_ID_DISPLAY=1`, `DDP_CHANNELS_PER_PACKET=1440`); lógica de recepção em `wled00/e131.cpp` (`handleDDPPacket`, validação de tipo comentada, cálculo de `start`/`stop` dividindo `channelOffset` pelo nº de canais, máquina de estado `ddpSeenPush`); e `FrameBuilder`/`Header`/`PacketRef` de `ddp-rs` (testes `single_frame_matches_expected_bytes` e `chunks_large_data_and_sets_push_on_last`, que fixam os bytes exatos `0x41,0x01,0x0D,0x01,0x00,0x00,0x00,0x00,0x00,0x09,...` e a sequência de push `[false,false,true]`).

Cada pacote transporta RGB contíguo de até 480 LEDs; frames maiores são segmentados em datagramas de no máximo 1440 bytes de dados, com `offset` avançando por pacote dentro do mesmo frame. Sem diffs nem pacote vazio de keep-alive (ao contrário de DNRGB, `Software/src/LedDeviceDnrgb.cpp:55-115`) — cada `setColors()` reenvia o frame completo com push no último pacote; é isso que o receptor espera para renderizar.

## 2. Itens rastreáveis

### Fase 1 — classe do device

- [x] Criar `Software/src/LedDeviceDdp.hpp` com `class LedDeviceDdp : public AbstractLedDeviceUdp`, espelhando a forma das classes UDP existentes (`Software/src/LedDeviceWarls.hpp:31-45`), e declarar o construtor `LedDeviceDdp(const QString& address, const QString& port, const uint8_t timeout, QObject * parent = 0)`, `QString name() const`, `int maxLedsCount()`, o slot `void setColors(const QList<QRgb> & colors, const bool rawColors)` e `void reinitBufferHeader()` protegido.

- [x] Criar `Software/src/LedDeviceDdp.cpp`: no construtor, encaminhar `address`, `port`, `timeout` e `parent` para `AbstractLedDeviceUdp`, como WARLS (`Software/src/LedDeviceWarls.cpp:30-32`); retornar o nome persistido `"ddp"` em `name()`; e retornar `MaximumNumberOfLeds::Ddp` em `maxLedsCount()`.

- [x] Em `LedDeviceDdp::setColors`, usar os pontos reais da base: chamar `resizeColorsBuffer(colors.count())`, aplicar `applyColorModifications(colors, m_colorsBuffer, rawColors)`, aplicar `applyDithering(m_colorsBuffer, 8)` somente quando `!rawColors`, construir os datagramas e enviar cada um com `writeBuffer(...)`, terminando com `emit commandCompleted(ok)`. Esses são o fluxo e os contratos de DRGB (`Software/src/LedDeviceDrgb.cpp:44-66`), enquanto `resizeColorsBuffer` limita pelo `maxLedsCount()` concreto (`Software/src/AbstractLedDeviceUdp.cpp:116-131`).

- [x] Definir a constante de segmentação como 480 LEDs / 1440 bytes RGB e percorrer `m_colorsBuffer` em blocos desse tamanho. Para cada bloco, serializar primeiro o cabeçalho DDP confirmado de 10 bytes e depois `r`, `g`, `b` em ordem, como o payload de DRGB (`Software/src/LedDeviceDrgb.cpp:58-63`), atualizando `sequence`, `offset`, `length` e flags exclusivamente conforme a especificação confirmada.

- [x] Implementar `reinitBufferHeader()` apenas para a parte invariável que a especificação confirmar; lembrar que a base o chama após `open()` (`Software/src/AbstractLedDeviceUdp.cpp:65-80`) e expõe `m_writeBufferHeader`, `m_writeBuffer`, `m_timeout` e `InfiniteTimeout` (`Software/src/AbstractLedDeviceUdp.hpp:53-63`). Não reutilizar `UdpDevice::Protocol`: esse enum contém os códigos WLED de WARLS/DRGB/DNRGB, não DDP (`Software/src/enums.hpp:122-130`).

- [x] Tratar explicitamente a porta DDP: configurar o default `4048` em settings e assegurar que a instância receba esse valor. A base converte a porta textual e, no caso inválido, atualmente volta para `21324` (`Software/src/AbstractLedDeviceUdp.cpp:37-43`); avaliar no patch se essa queda deve permanecer genérica ou se a base precisa de um ponto de extensão para preservar o default específico de DDP, sem alterar a semântica dos devices existentes.

### Fase 2 — registro e settings

- [x] Em `Software/src/SettingsDefaults.hpp`, acrescentar `DDP` a `SUPPORTED_DEVICES` nos dois ramos condicionais (`Software/src/SettingsDefaults.hpp:34-38`) e criar `SettingsScope::Main::Ddp` junto dos namespaces UDP (`Software/src/SettingsDefaults.hpp:136-156`) com `NumberOfLedsDefault`, `AddressDefault`, `PortDefault = QStringLiteral("4048")` e `TimeoutDefault` coerente com o padrão escolhido para DDP.

- [x] Em `Software/src/enums.hpp`, inserir `DeviceTypeDdp` antes de `DeviceTypesCount` (`Software/src/enums.hpp:82-96`); inserir `Ddp` em `MaximumNumberOfLeds::Devices`, usar o limite confirmado de 480 LEDs por datagrama apenas como limite de pacote (não como máximo total do device), e recalcular `AbsoluteMaximum` se o máximo global mudar (`Software/src/enums.hpp:99-119`). A constante final de máximo configurável deve ser definida após a especificação/receiver confirmar a capacidade total, porque DDP suporta segmentação.

- [x] Estender o modelo de settings que sustenta o registro: criar `Main::Key::Ddp` e `Main::Value::ConnectedDevice::DdpDevice` em `Software/src/Settings.cpp:132-175`; inicializar número de LEDs, consumo, fonte e `Address`/`Port`/`Timeout` em `Settings::initSettings()` seguindo os blocos DRGB/DNRGB/WARLS (`Software/src/Settings.cpp:346-381`); e adicionar os getters/setters DDP e sinais correspondentes em `Software/src/Settings.hpp:133-150,324-346` e suas implementações de `Software/src/Settings.cpp:910-1015`.

- [x] Registrar `DeviceTypeDdp` em todos os mapas de `Settings::initDevicesMap()` — nome, `NumberOfLeds`, `LedMilliAmps` e `PowerSupplyAmps` — reproduzindo o conjunto UDP existente (`Software/src/Settings.cpp:2258-2292`), e nos três `switch` de emissão de mudanças de LEDs/corrente/fonte (`Software/src/Settings.cpp:1071-1102`, `Software/src/Settings.cpp:1138-1169`, `Software/src/Settings.cpp:1206-1237`).

- [x] Incluir `LedDeviceDdp.hpp` em `Software/src/LedDeviceManager.cpp` ao lado das classes UDP (`Software/src/LedDeviceManager.cpp:36-42`) e adicionar o `case SupportedDevices::DeviceTypeDdp` em `LedDeviceManager::createLedDevice(...)` (`Software/src/LedDeviceManager.cpp:464-525`) para instanciar `LedDeviceDdp(Settings::getDdpAddress(), Settings::getDdpPort(), Settings::getDdpTimeout())`. Este é o local exato de criação por tipo; o resultado ainda recebe corrente e fonte genericamente (`Software/src/LedDeviceManager.cpp:527-530`).

### Fase 3 — UI

- [x] Adicionar em `Software/src/wizard/SelectDevicePage.ui` um radio button `rbDdp`, com texto que indique `DDP (UDP, ... LEDs)` sem prometer um máximo total antes de defini-lo; a própria enumeração avisa que os limites apresentados na UI precisam acompanhar `MaximumNumberOfLeds` (`Software/src/enums.hpp:99-102`), e os controles UDP atuais estão em `Software/src/wizard/SelectDevicePage.ui:76-116`.

- [x] Em `SelectDevicePage::initializePage()` registrar o campo `isDdp` e restaurar sua seleção quando `deviceType == SupportedDevices::DeviceTypeDdp`; limpá-lo em `cleanupPage()` e incluí-lo no ramo UDP de `nextId()` para chegar a `Page_ConfigureUdpDevice` (`Software/src/wizard/SelectDevicePage.cpp:47-78,88-96`).

- [x] Estender `Software/src/wizard/ConfigureUdpDevicePage.cpp`: incluir `LedDeviceDdp.hpp`; carregar `getDdpAddress()`, `getDdpPort()` e `getDdpTimeout()` quando `isDdp` estiver marcado; e criar `LedDeviceDdp(address, port, timeout)` em `validatePage()`. Esses três pontos são hoje os ramos DRGB/DNRGB/WARLS (`Software/src/wizard/ConfigureUdpDevicePage.cpp:32-35,47-79,88-110`) e garantem que o wizard de fato consiga testar o novo device, inclusive a porta padrão 4048.

### Fase 4 — build

- [x] Registrar `LedDeviceDdp.cpp` em `SOURCES` e `LedDeviceDdp.hpp` em `HEADERS` de `Software/src/src.pro`, juntos dos arquivos DRGB/DNRGB/WARLS (`Software/src/src.pro:300-303,352-355`), para que qmake/MOC compilem a classe `Q_OBJECT`.

## 3. Testes

`Software/tests/tests.pro` hoje lista testes de API, settings, captura, matemática, versão e linha de comando, mas não inclui nenhuma classe UDP nem qualquer teste de empacotamento de protocolo (`Software/tests/tests.pro:46-80`). Os itens abaixo serão, portanto, os **primeiros testes de empacotamento de protocolo UDP do repositório**.

- [x] Adicionar uma classe de teste QtTest para o empacotador DDP e registrá-la em `Software/tests/tests.pro`; estruturar a classe de forma que possa inspecionar os datagramas sem enviar tráfego de rede real (por exemplo, separando a construção de pacote da chamada existente `writeBuffer`, cujo retorno é a confirmação de I/O em `Software/src/AbstractLedDeviceUdp.cpp:139-156`).

- [x] Cobrir um frame RGB com `N <= 480` LEDs: exatamente um datagrama, 10 bytes de cabeçalho mais `3 * N` bytes de payload, RGB na mesma ordem de entrada, `length` correspondente e campos `data type`/`ID` conforme os valores confirmados da especificação.

- [x] Cobrir exatamente 480 LEDs: payload de 1440 bytes e nenhum segundo datagrama, verificando o limite máximo de um pacote.

- [x] Cobrir 481 LEDs e um frame maior com múltiplos de 480: verificar a quantidade de pacotes, os tamanhos de payload (1440 e restante), a progressão de `offset` entre blocos e a regra de `sequence` confirmada na especificação.

- [x] Verificar os limites de `length` e os bytes de cabeçalho com vetores de referência obtidos da especificação oficial ou de um receiver DDP de teste; confirmar endianness de `offset`/`length` e a serialização de todos os campos antes de aceitar a implementação.

- [x] Se a especificação confirmar flag de push/último pacote, testar que ela aparece apenas no datagrama final do frame multipartido e com o valor correto também no frame de pacote único; se não se aplicar ao modo RGB escolhido, registrar o motivo e não criar uma flag presumida.

- [x] Testar o comportamento escolhido para dois frames idênticos e para desligamento (`switchOffLeds()` cria um frame preto e chama `setColors(..., true)` em `Software/src/AbstractLedDeviceUdp.cpp:106-114`): devem obedecer à regra DDP confirmada para reenvio/keep-alive, sem herdar automaticamente o pacote vazio de timeout específico de DNRGB (`Software/src/LedDeviceDnrgb.cpp:108-115`).

## 4. Critérios de aceite

| Área | Evidência de conclusão |
|---|---|
| Especificação | Layout binário e semântica de flags/sequence/offset/length conferidos na fonte DDP antes do merge. |
| Device | `LedDeviceDdp` compila, usa a base `AbstractLedDeviceUdp` e segmenta RGB em no máximo 480 LEDs por datagrama. |
| Integração | DDP aparece na lista suportada, persiste address/porta/timeout, é instanciado por `LedDeviceManager` e pode ser selecionado/configurado pelo wizard. |
| Build e testes | `src.pro` lista os novos arquivos; os testes de pacotes DDP são executáveis e cobrem pacote único, multipartido e flags condicionais. |
