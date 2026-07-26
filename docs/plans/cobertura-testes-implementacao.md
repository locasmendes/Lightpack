# Plano de implementação — cobertura de testes

Este plano transforma os achados da auditoria de [cobertura de testes](../cobertura-testes.md) em trabalho implementável e priorizado. O primeiro objetivo é fazer a suíte existente falhar no CI quando houver regressão; os três seguintes protegem a conversão de cor, o pipeline de saída e o pós-processamento da captura. O contexto de jobs e plataformas está em [CI, build e release](../ci-build-release.md), especialmente o fato de que o workflow atual só compila a suíte no Windows e não a executa.

As fases devem entrar nessa ordem. Cada uma precisa deixar o repositório verde localmente e no novo job Linux antes de a próxima depender dela. As citações abaixo indicam o ponto exato a alterar ou a cobrir.

---

## 1. Itens rastreáveis

### Fase 1 — executar `LightpackTests` no CI

- [ ] Adicionar ao `.github/workflows/ci.yml` um job Linux independente, por exemplo `test-lightpack`, que instale Qt com `widgets`, `network` e `testlib`, além de `xvfb`, e rode a suíte em ambiente sem display. O workflow atual só tem jobs de build/pacote (`ci.yml:12-249`) e o build Windows termina em `MSBuild.exe` sem chamar o binário de teste (`ci.yml:155-160`).
- [ ] No job, compilar as dependências estáticas de `LightpackTests` antes da suíte (`Software/math/math.pro` e `Software/grab/grab.pro`) e então executar `qmake tests.pro && make` dentro de `Software/tests`, pois o projeto de testes linka explicitamente `-lprismatik-math -lgrab` (`Software/tests/tests.pro:29-31`). Registrar os comandos e versões de Qt no log do Actions.
- [ ] Executar `Software/tests/bin/LightpackTests` com `QT_QPA_PLATFORM=offscreen`; se alguma dependência de `QApplication` ainda exigir servidor X, encapsular somente essa execução em `xvfb-run -a`. O entry point cria `QApplication`, não apenas `QCoreApplication` (`Software/tests/TestsMain.cpp:18-22`).
- [ ] Manter o job Linux explícito em vez de depender do `SUBDIRS` raiz: `tests` hoje só é adicionado em Windows (`Software/Lightpack.pro:36-40`). Documentar no YAML por que o job invoca `tests.pro` diretamente, ou, se a decisão for tornar `tests` multiplataforma no projeto-raiz, remover essa condição e validar os três SOs afetados.
- [ ] Fazer o passo de execução falhar o job quando qualquer suíte falhar e publicar o log de QtTest como artefato em caso de falha. `TestsMain.cpp` agrega o retorno de cada `QTest::qExec` (`Software/tests/TestsMain.cpp:37-49`), portanto o critério deve ser o código de saída do processo, não procura textual por “FAILED”.
- [ ] Verificar a estabilidade de `LightpackApiTest` no runner: reservar ou tornar configurável a porta TCP fixa 3636 antes de depender do job; se não for possível sem alterar o protocolo do teste, detectar a ocupação e emitir diagnóstico inequívoco. A auditoria identifica esse teste de integração como dependente dessa porta ([`../cobertura-testes.md`](../cobertura-testes.md), §2.3).

### Fase 2 — regressão numérica de `PrismatikMath`

- [ ] Expandir `LightpackMathTest` em `Software/tests/lightpackmathtest.{hpp,cpp}` com slots nomeados por comportamento, preservando `testCase1()` até que seus checks HSV sejam migrados para um nome descritivo. Hoje há um único slot e ele só cobre HSV (`Software/tests/lightpackmathtest.hpp:6-14`, `Software/tests/lightpackmathtest.cpp:9-17`).
- [ ] Adicionar dados de teste para `PrismatikMath::toXyz(const StructRgb&)`, `toLab(const StructRgb&)`, `toLab(const StructXyz&)`, `toRgb(const StructXyz&)` e os overloads Lab/RGB declarados em `Software/math/include/PrismatikMath.hpp:48-53`.
- [ ] Criar um pequeno helper de asserção para `StructRgb`, `StructXyz` e `StructLab`: igualdade exata para canais inteiros e tolerância absoluta explicitada para `double`. `StructRgb` é RGB de 12 bits e `StructLab` quantiza L/a/b em `unsigned char`/`char` (`Software/math/include/colorspace_types.h:30-48`), logo um round-trip não deve exigir igualdade bit a bit em todos os caminhos.
- [ ] Versionar no próprio teste uma tabela de vetores de referência com a fonte, iluminante D65 e observador de 2°. Gerar esses valores uma vez por uma implementação independente, como `colour-science`/`colour` em Python, convertendo os valores RGB de 8 para a escala 0–4095 antes de chamar o código; revisar manualmente a tabela antes de torná-la golden. Não copiar constantes da própria implementação: ela aplica EOTF sRGB e matriz D65 em `PrismatikMath.cpp:196-225` e converteria um erro compartilhado em falso positivo.
- [ ] Incluir no comentário da tabela ao menos o vetor RGB conhecido escolhido e seu Lab esperado aproximado, extraído da ferramenta independente e com precisão/tolerância declaradas. Até a geração reproduzível dessa tabela, não inserir números “de memória” no teste ou neste plano.
- [ ] Acrescentar os novos `.cpp`/`.hpp` a `HEADERS`/`SOURCES` de `Software/tests/tests.pro` se a expansão virar uma suíte própria; se permanecer em `LightpackMathTest`, manter somente os arquivos já listados (`Software/tests/tests.pro:46-80`) e garantir que o construtor continue registrado em `TestsMain.cpp:32`.

### Fase 3 — testes do pipeline de `AbstractLedDevice`

- [ ] Criar `AbstractLedDeviceTest` e um `TestLedDevice` mínimo, somente para teste, que implemente as operações puras de `AbstractLedDevice` e exponha wrappers controlados para `applyColorModifications` e `applyDithering`. O método-alvo e os campos de configuração são `protected` (`Software/src/AbstractLedDevice.hpp:98-115`), portanto o wrapper evita tornar API de produção pública.
- [ ] Preparar o alvo de testes para compilar `Software/src/AbstractLedDevice.cpp` e suas dependências, e registrar a nova suíte em `tests.pro` e em `TestsMain.cpp`, onde as cinco suítes atuais são instanciadas (`Software/tests/tests.pro:66-80`, `Software/tests/TestsMain.cpp:26-35`). Não instanciar hardware nem uma classe `LedDevice*` concreta.
- [ ] Adicionar um caso de expansão pura: uma entrada `QRgb` de canais distintos deve resultar em `canal * (4095 / 255.0)` truncado para o `unsigned` de `StructRgb`; testar também `rawColors=true`, que mantém somente essa conversão e retorna cedo (`Software/src/AbstractLedDevice.cpp:121-133`).
- [ ] Adicionar casos isolados e um caso composto para gamma, limiar Lab, brilho, white balance, `brightnessCap` e limite da fonte/corrente. Cada caso deve configurar somente a etapa em foco, usar vetor de entrada pequeno e verificar a lista inteira de `StructRgb` de 12 bits, incluindo ordem e quantidade de LEDs. A ordem de produção é: gamma, Lab threshold, brilho, WB, cap e limitação global de corrente (`Software/src/AbstractLedDevice.cpp:127-188`); o teste composto deve provar essa ordem, não uma ordem idealizada.
- [ ] Cobrir os dois ramos do limiar Lab: abaixo do limiar com mínimo desabilitado zera RGB (`Software/src/AbstractLedDevice.cpp:141-160`); com mínimo habilitado, ajusta L e aproxima a/b da cor média. Os esperados numéricos desse segundo ramo devem vir da tabela independente da Fase 2 ou de um gerador golden versionado, pois ele chama `toLab`/`toRgb` (`Software/src/AbstractLedDevice.cpp:135,141-154`).
- [ ] Cobrir WB apenas quando a quantidade de `WBAdjustment` coincidir com a quantidade de cores e o caso de cardinalidade divergente, que deve ignorar WB (`Software/src/AbstractLedDevice.cpp:117,164-168`). Cobrir cap com uma soma acima do teto e corrente/fonte com ao menos dois LEDs cuja corrente estimada exceda `m_powerSupplyAmps` (`Software/src/AbstractLedDevice.cpp:169-187`).
- [ ] Testar `applyDithering` em slot separado, com profundidade de cor e sequência que exercitem o transporte de erro com dither ligado e desligado. Não declarar que ele é etapa de `applyColorModifications`: o método é separado (`Software/src/AbstractLedDevice.cpp:191-230`) e não é chamado pelo método-alvo.

### Fase 4 — infraestrutura mínima para `GrabManager::handleGrabbedColors`

- [ ] Introduzir um ponto de injeção mínimo e explícito em `GrabManager` para os colaboradores observáveis pelo teste: um `GrabberBase` controlado que emite `frameGrabAttempted(GrabResultOk)` e um consumidor/spy do sinal `updateLedsColors(const QList<QRgb>&)`. O método é `private slot` (`Software/src/GrabManager.hpp:85-93`) e publica resultado por sinal (`Software/src/GrabManager.cpp:491-494`), portanto o teste deve acioná-lo pela mesma fronteira de eventos de produção, sem acessar membros privados por macro.
- [ ] Preferir uma factory/injeção de `GrabberBase` limitada ao construtor ou a um setter de teste com visibilidade de teste, em vez de mockar todos os grabbers de plataforma. A interface abstrata já permite uma implementação falsa ao exigir `grabScreens`, `reallocate` e `screensWithWidgets` (`Software/grab/include/GrabberBase.hpp:79-123`); o fake deve devolver frames/cenários determinísticos e não abrir tela, DXGI, X11 ou hardware.
- [ ] Criar `GrabManagerTest` e fixtures Qt headless com LEDs/áreas habilitados conhecidos. Completar `tests.pro` com os fontes reais necessários de `GrabManager` e seus colaboradores de UI, ou extrair somente a lógica de pós-processamento para uma dependência sem `QWidget` se o link mostrar que esse é o menor corte. Registrar a suíte em `TestsMain.cpp`.
- [ ] Implementar a primeira bateria para `handleGrabbedColors`: passthrough de cores novas; média somente das áreas habilitadas; overbrighten com saturação em 255; emissão suprimida quando “send only if changed” está ativo e repetição idêntica; emissão forçada quando está desativado; e retorno sem emissão durante pausa. Esses são ramos concretos em `Software/src/GrabManager.cpp:416-502`.
- [ ] Adicionar, como segundo incremento da mesma infraestrutura, testes de temperatura de cor e de redução de luz azul com doubles/fakes separados. Não ligar `BlueLightReduction::Client` real ao teste: o caminho só é escolhido quando a opção e o cliente existem (`Software/src/GrabManager.cpp:437-442`), portanto uma abstração injetável também deve permitir verificar precedência de temperatura sobre redução azul.
- [ ] Reutilizar o padrão do único mock existente apenas onde ele for adequado: `SettingsWindowMockup` é um `QObject` que captura argumentos e marca `m_isDone` em slots (`Software/tests/SettingsWindowMockup.hpp:32-68`, `Software/tests/SettingsWindowMockup.cpp:28-74`). O novo fake deve ter a mesma simplicidade observável, mas não acoplar `GrabManagerTest` à UI/API.

---

## 2. Testes

| Fase | Caso específico | Oráculo/resultado esperado |
|---|---|---|
| CI | Executar todas as suítes registradas pelo binário sob `offscreen` (e `xvfb-run` se necessário) | Código de saída zero; uma falha de `QTest::qExec` torna o job vermelho. |
| Math | Primárias, branco, preto e pelo menos uma cor não extrema em RGB 12-bit para `RGB -> XYZ -> Lab` | Comparar XYZ com tolerância documentada e Lab quantizado contra a tabela golden produzida por `colour-science` em D65/2°. A fonte e a data/versão do gerador ficam junto à tabela. |
| Math | `RGB -> XYZ -> RGB` e `RGB -> Lab -> RGB` para os mesmos vetores, incluindo proximidade dos limiares de EOTF | Cada canal volta dentro de tolerância de quantização definida pelo teste; testar limites próximos a `0.04045` e `0.0031308`, usados nos ramos de conversão (`Software/math/PrismatikMath.cpp:202-215,301-314`). |
| Math | `Lab -> XYZ -> Lab` para vetores Lab representáveis | L/a/b retornam iguais ou com a tolerância de uma unidade explicitada, sem overflow de `char`. |
| Device | Expansão 8-bit -> 12-bit e `rawColors=true` | Para cada canal, `unsigned(qCanal * (4095 / 255.0))`; nenhuma gamma, threshold, brilho, WB, cap ou corrente é aplicada. |
| Device | Gamma | Vetor de canais distintos, com demais controles neutros, coincide com golden independente de `gammaCorrection`; inclui gamma padrão e um gamma não unitário. |
| Device | Lab threshold | Cor abaixo do limiar vira `{0,0,0}` quando mínimo está desligado; com mínimo ligado, usar esperado golden baseado na Fase 2 para L/a/b e RGB resultante. |
| Device | Brilho, WB e cap | Casos unitários mostram fator de brilho, coeficientes WB por LED e reescala proporcional quando a soma passa o cap. Inclui lista WB de tamanho incorreto sem alteração por WB. |
| Device | Corrente/fonte e dither | Dois LEDs excedem `m_powerSupplyAmps` e são reescalados pelo mesmo ratio; `applyDithering` verifica saída por profundidade e carry com dither on/off, em teste separado. |
| GrabManager | Pass-through, média, overbrighten, diff de quadro e pausa | `QSignalSpy` observa exatamente zero ou uma emissão e a lista RGB esperada em cada cenário; o fake não acessa hardware nem servidor gráfico. |
| GrabManager | Temperatura vs. blue-light reduction | Fake de cada transformador confirma que temperatura tem precedência quando ambas as opções estão habilitadas, refletindo o `if`/`else if` de produção. |

Um formato ilustrativo para tornar a divisão inequívoca é:

```cpp
private slots:
    void rgbXyzLab_referenceVectors_data();
    void rgbXyzLab_referenceVectors();
    void applyColorModifications_rawExpansion();
    void applyColorModifications_labThreshold_data();
    void applyColorModifications_labThreshold();
    void applyDithering_carries();
    void handleGrabbedColors_emitsOnlyOnChange();
```

Os valores golden de Lab/XYZ e dos ramos que deles dependem devem ser revisados como dados de teste, não calculados chamando `PrismatikMath` durante o próprio teste. Isso preserva a capacidade de detectar regressões na matriz, no EOTF e na quantização.

---

## 3. Critérios de aceite e sequência de entrega

1. A Fase 1 passa em pull request no Linux headless e continua permitindo o build Windows existente.
2. A Fase 2 introduz os vetores golden e todos os overloads de conversão listados, com tolerâncias justificadas.
3. A Fase 3 cobre cada etapa real de `applyColorModifications`; dither fica coberto sem alegar ser chamado por ela.
4. A Fase 4 consegue testar os ramos iniciais de `handleGrabbedColors` sem hardware, display físico ou `#define private public`.
5. Cada fase acrescenta casos ao job da Fase 1; nenhum teste novo fica apenas compilado, repetindo a lacuna documentada em [`../cobertura-testes.md`](../cobertura-testes.md), §7.
