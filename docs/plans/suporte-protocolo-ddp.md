# Plano de implementação — suporte ao protocolo DDP

Este plano adiciona DDP (Distributed Display Protocol) como mais um device UDP do Prismatik, seguindo a arquitetura de WARLS, DRGB e DNRGB. Ele complementa o contexto de transporte já registrado em [`firmware-hardware-datados.md`](../firmware-hardware-datados.md#33-transporte-usb-hid-full-speed-vs-alternativas-mais-rápidas), que confirma que DDP ainda não é implementado no repositório (`docs/firmware-hardware-datados.md:149-155`).

## 1. Premissas do protocolo e limite de confirmação

DDP usa UDP na porta padrão `4048`. Cada datagrama terá cabeçalho de 10 bytes e payload RGB de no máximo 480 pixels (1440 bytes). A estrutura lógica do cabeçalho é:

| Campo | Papel no pacote | Estado da confirmação |
|---|---|---|
| `flags` | Sinaliza as opções de entrega; inclui o comportamento de envio/último pacote somente se a especificação o definir para este fluxo. | A confirmar na especificação |
| `sequence` | Identifica a sequência do datagrama. | A confirmar na especificação |
| `data type` | Indica o tipo/formato dos dados RGB. | A confirmar na especificação |
| `ID` | Identifica o destino/segmento DDP. | A confirmar na especificação |
| `offset` | Indica a posição inicial dos dados no stream. | A confirmar na especificação |
| `length` | Indica o tamanho do payload. | A confirmar na especificação |

O acesso à fonte indicada, [3waylabs.com/ddp](http://www.3waylabs.com/ddp/), retornou `502 Bad Gateway` durante a elaboração deste plano. Antes de implementar, confirmar nessa especificação o layout binário exato: ordem e largura de cada campo, endianness de `offset` e `length`, valores de `data type`, semântica e bits de `flags` (inclusive push/last-packet), política de incremento/retorno de `sequence` e valor de `ID`. Não inferir essas posições ou bits a partir do cabeçalho total de 10 bytes.

Para o payload já confirmado, cada pacote transportará RGB contíguo de até 480 LEDs; frames maiores serão segmentados em datagramas de no máximo 1440 bytes de dados, com `offset` apropriado a cada segmento. A decisão sobre diffs, keep-alive e push/last-packet deve ser tomada somente depois dessa confirmação, em contraste com o envio diferencial e o pacote vazio de DNRGB (`Software/src/LedDeviceDnrgb.cpp:55-115`).

## 2. Itens rastreáveis

### Fase 1 — classe do device

- [ ] Criar `Software/src/LedDeviceDdp.hpp` com `class LedDeviceDdp : public AbstractLedDeviceUdp`, espelhando a forma das classes UDP existentes (`Software/src/LedDeviceWarls.hpp:31-45`), e declarar o construtor `LedDeviceDdp(const QString& address, const QString& port, const uint8_t timeout, QObject * parent = 0)`, `QString name() const`, `int maxLedsCount()`, o slot `void setColors(const QList<QRgb> & colors, const bool rawColors)` e `void reinitBufferHeader()` protegido.

- [ ] Criar `Software/src/LedDeviceDdp.cpp`: no construtor, encaminhar `address`, `port`, `timeout` e `parent` para `AbstractLedDeviceUdp`, como WARLS (`Software/src/LedDeviceWarls.cpp:30-32`); retornar o nome persistido `"ddp"` em `name()`; e retornar `MaximumNumberOfLeds::Ddp` em `maxLedsCount()`.

- [ ] Em `LedDeviceDdp::setColors`, usar os pontos reais da base: chamar `resizeColorsBuffer(colors.count())`, aplicar `applyColorModifications(colors, m_colorsBuffer, rawColors)`, aplicar `applyDithering(m_colorsBuffer, 8)` somente quando `!rawColors`, construir os datagramas e enviar cada um com `writeBuffer(...)`, terminando com `emit commandCompleted(ok)`. Esses são o fluxo e os contratos de DRGB (`Software/src/LedDeviceDrgb.cpp:44-66`), enquanto `resizeColorsBuffer` limita pelo `maxLedsCount()` concreto (`Software/src/AbstractLedDeviceUdp.cpp:116-131`).

- [ ] Definir a constante de segmentação como 480 LEDs / 1440 bytes RGB e percorrer `m_colorsBuffer` em blocos desse tamanho. Para cada bloco, serializar primeiro o cabeçalho DDP confirmado de 10 bytes e depois `r`, `g`, `b` em ordem, como o payload de DRGB (`Software/src/LedDeviceDrgb.cpp:58-63`), atualizando `sequence`, `offset`, `length` e flags exclusivamente conforme a especificação confirmada.

- [ ] Implementar `reinitBufferHeader()` apenas para a parte invariável que a especificação confirmar; lembrar que a base o chama após `open()` (`Software/src/AbstractLedDeviceUdp.cpp:65-80`) e expõe `m_writeBufferHeader`, `m_writeBuffer`, `m_timeout` e `InfiniteTimeout` (`Software/src/AbstractLedDeviceUdp.hpp:53-63`). Não reutilizar `UdpDevice::Protocol`: esse enum contém os códigos WLED de WARLS/DRGB/DNRGB, não DDP (`Software/src/enums.hpp:122-130`).

- [ ] Tratar explicitamente a porta DDP: configurar o default `4048` em settings e assegurar que a instância receba esse valor. A base converte a porta textual e, no caso inválido, atualmente volta para `21324` (`Software/src/AbstractLedDeviceUdp.cpp:37-43`); avaliar no patch se essa queda deve permanecer genérica ou se a base precisa de um ponto de extensão para preservar o default específico de DDP, sem alterar a semântica dos devices existentes.

### Fase 2 — registro e settings

- [ ] Em `Software/src/SettingsDefaults.hpp`, acrescentar `DDP` a `SUPPORTED_DEVICES` nos dois ramos condicionais (`Software/src/SettingsDefaults.hpp:34-38`) e criar `SettingsScope::Main::Ddp` junto dos namespaces UDP (`Software/src/SettingsDefaults.hpp:136-156`) com `NumberOfLedsDefault`, `AddressDefault`, `PortDefault = QStringLiteral("4048")` e `TimeoutDefault` coerente com o padrão escolhido para DDP.

- [ ] Em `Software/src/enums.hpp`, inserir `DeviceTypeDdp` antes de `DeviceTypesCount` (`Software/src/enums.hpp:82-96`); inserir `Ddp` em `MaximumNumberOfLeds::Devices`, usar o limite confirmado de 480 LEDs por datagrama apenas como limite de pacote (não como máximo total do device), e recalcular `AbsoluteMaximum` se o máximo global mudar (`Software/src/enums.hpp:99-119`). A constante final de máximo configurável deve ser definida após a especificação/receiver confirmar a capacidade total, porque DDP suporta segmentação.

- [ ] Estender o modelo de settings que sustenta o registro: criar `Main::Key::Ddp` e `Main::Value::ConnectedDevice::DdpDevice` em `Software/src/Settings.cpp:132-175`; inicializar número de LEDs, consumo, fonte e `Address`/`Port`/`Timeout` em `Settings::initSettings()` seguindo os blocos DRGB/DNRGB/WARLS (`Software/src/Settings.cpp:346-381`); e adicionar os getters/setters DDP e sinais correspondentes em `Software/src/Settings.hpp:133-150,324-346` e suas implementações de `Software/src/Settings.cpp:910-1015`.

- [ ] Registrar `DeviceTypeDdp` em todos os mapas de `Settings::initDevicesMap()` — nome, `NumberOfLeds`, `LedMilliAmps` e `PowerSupplyAmps` — reproduzindo o conjunto UDP existente (`Software/src/Settings.cpp:2258-2292`), e nos três `switch` de emissão de mudanças de LEDs/corrente/fonte (`Software/src/Settings.cpp:1071-1102`, `Software/src/Settings.cpp:1138-1169`, `Software/src/Settings.cpp:1206-1237`).

- [ ] Incluir `LedDeviceDdp.hpp` em `Software/src/LedDeviceManager.cpp` ao lado das classes UDP (`Software/src/LedDeviceManager.cpp:36-42`) e adicionar o `case SupportedDevices::DeviceTypeDdp` em `LedDeviceManager::createLedDevice(...)` (`Software/src/LedDeviceManager.cpp:464-525`) para instanciar `LedDeviceDdp(Settings::getDdpAddress(), Settings::getDdpPort(), Settings::getDdpTimeout())`. Este é o local exato de criação por tipo; o resultado ainda recebe corrente e fonte genericamente (`Software/src/LedDeviceManager.cpp:527-530`).

### Fase 3 — UI

- [ ] Adicionar em `Software/src/wizard/SelectDevicePage.ui` um radio button `rbDdp`, com texto que indique `DDP (UDP, ... LEDs)` sem prometer um máximo total antes de defini-lo; a própria enumeração avisa que os limites apresentados na UI precisam acompanhar `MaximumNumberOfLeds` (`Software/src/enums.hpp:99-102`), e os controles UDP atuais estão em `Software/src/wizard/SelectDevicePage.ui:76-116`.

- [ ] Em `SelectDevicePage::initializePage()` registrar o campo `isDdp` e restaurar sua seleção quando `deviceType == SupportedDevices::DeviceTypeDdp`; limpá-lo em `cleanupPage()` e incluí-lo no ramo UDP de `nextId()` para chegar a `Page_ConfigureUdpDevice` (`Software/src/wizard/SelectDevicePage.cpp:47-78,88-96`).

- [ ] Estender `Software/src/wizard/ConfigureUdpDevicePage.cpp`: incluir `LedDeviceDdp.hpp`; carregar `getDdpAddress()`, `getDdpPort()` e `getDdpTimeout()` quando `isDdp` estiver marcado; e criar `LedDeviceDdp(address, port, timeout)` em `validatePage()`. Esses três pontos são hoje os ramos DRGB/DNRGB/WARLS (`Software/src/wizard/ConfigureUdpDevicePage.cpp:32-35,47-79,88-110`) e garantem que o wizard de fato consiga testar o novo device, inclusive a porta padrão 4048.

### Fase 4 — build

- [ ] Registrar `LedDeviceDdp.cpp` em `SOURCES` e `LedDeviceDdp.hpp` em `HEADERS` de `Software/src/src.pro`, juntos dos arquivos DRGB/DNRGB/WARLS (`Software/src/src.pro:300-303,352-355`), para que qmake/MOC compilem a classe `Q_OBJECT`.

## 3. Testes

`Software/tests/tests.pro` hoje lista testes de API, settings, captura, matemática, versão e linha de comando, mas não inclui nenhuma classe UDP nem qualquer teste de empacotamento de protocolo (`Software/tests/tests.pro:46-80`). Os itens abaixo serão, portanto, os **primeiros testes de empacotamento de protocolo UDP do repositório**.

- [ ] Adicionar uma classe de teste QtTest para o empacotador DDP e registrá-la em `Software/tests/tests.pro`; estruturar a classe de forma que possa inspecionar os datagramas sem enviar tráfego de rede real (por exemplo, separando a construção de pacote da chamada existente `writeBuffer`, cujo retorno é a confirmação de I/O em `Software/src/AbstractLedDeviceUdp.cpp:139-156`).

- [ ] Cobrir um frame RGB com `N <= 480` LEDs: exatamente um datagrama, 10 bytes de cabeçalho mais `3 * N` bytes de payload, RGB na mesma ordem de entrada, `length` correspondente e campos `data type`/`ID` conforme os valores confirmados da especificação.

- [ ] Cobrir exatamente 480 LEDs: payload de 1440 bytes e nenhum segundo datagrama, verificando o limite máximo de um pacote.

- [ ] Cobrir 481 LEDs e um frame maior com múltiplos de 480: verificar a quantidade de pacotes, os tamanhos de payload (1440 e restante), a progressão de `offset` entre blocos e a regra de `sequence` confirmada na especificação.

- [ ] Verificar os limites de `length` e os bytes de cabeçalho com vetores de referência obtidos da especificação oficial ou de um receiver DDP de teste; confirmar endianness de `offset`/`length` e a serialização de todos os campos antes de aceitar a implementação.

- [ ] Se a especificação confirmar flag de push/último pacote, testar que ela aparece apenas no datagrama final do frame multipartido e com o valor correto também no frame de pacote único; se não se aplicar ao modo RGB escolhido, registrar o motivo e não criar uma flag presumida.

- [ ] Testar o comportamento escolhido para dois frames idênticos e para desligamento (`switchOffLeds()` cria um frame preto e chama `setColors(..., true)` em `Software/src/AbstractLedDeviceUdp.cpp:106-114`): devem obedecer à regra DDP confirmada para reenvio/keep-alive, sem herdar automaticamente o pacote vazio de timeout específico de DNRGB (`Software/src/LedDeviceDnrgb.cpp:108-115`).

## 4. Critérios de aceite

| Área | Evidência de conclusão |
|---|---|
| Especificação | Layout binário e semântica de flags/sequence/offset/length conferidos na fonte DDP antes do merge. |
| Device | `LedDeviceDdp` compila, usa a base `AbstractLedDeviceUdp` e segmenta RGB em no máximo 480 LEDs por datagrama. |
| Integração | DDP aparece na lista suportada, persiste address/porta/timeout, é instanciado por `LedDeviceManager` e pode ser selecionado/configurado pelo wizard. |
| Build e testes | `src.pro` lista os novos arquivos; os testes de pacotes DDP são executáveis e cobrem pacote único, multipartido e flags condicionais. |
