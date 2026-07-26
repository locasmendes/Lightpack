# Plano de implementação — higiene de CI e release

Este plano transforma em mudanças verificáveis os riscos levantados na auditoria [CI, build e release](../ci-build-release.md). Ele cobre execução efetiva de testes, consistência da versão e a consolidação do CI de macOS/release; não altera workflows nem publica artefatos nesta etapa de planejamento.

---

## 1. Itens rastreáveis

### Fase 1 — Testes no CI (rodar `LightpackTests` headless)

- [ ] Adicionar ao `.github/workflows/ci.yml` um job Linux dedicado, por exemplo `test-linux`, em `ubuntu-20.04`, para não acoplar a primeira execução da suíte a um artefato de pacote. O workflow atual tem jobs de empacotamento Linux, firmware, Windows e macOS, mas nenhum passo de execução de testes (`.github/workflows/ci.yml:12-249`).
- [ ] Instalar no job as dependências de compilação Qt/QMake, bibliotecas de desenvolvimento já requeridas pelo projeto e `xvfb`; documentar as versões/pacotes escolhidos no próprio passo YAML para que a imagem do runner seja reproduzível.
- [ ] Compilar primeiro as dependências Linux do projeto (`math` e `grab`) e, em seguida, gerar e compilar explicitamente `Software/tests/tests.pro`; o projeto-raiz só inclui `tests` sob `win32` (`Software/Lightpack.pro:36-40`), embora a suíte seja portável e declare `widgets`, `network` e `testlib` (`Software/tests/tests.pro:7-12`).
- [ ] Executar o binário produzido `LightpackTests` em sessão headless com `QT_QPA_PLATFORM=offscreen` e `xvfb-run -a`, sob um limite global explícito (por exemplo, `timeout --signal=TERM --kill-after=10s 120s`). Publicar o log do teste como artefato quando o passo falhar para preservar a causa de falhas intermitentes.
- [ ] Antes de iniciar a suíte, verificar que a porta TCP `3636` está livre (por exemplo, com `ss -ltn`); se estiver ocupada, falhar com diagnóstico do processo/soquete em vez de executar contra um servidor alheio. `LightpackApiTest` cria `ApiServer(3636)` e se conecta a `127.0.0.1:3636` (`Software/tests/LightpackApiTest.cpp:60,93`); o construtor chama `listen(QHostAddress::Any, m_apiPort)` e aborta se não conseguir escutar (`Software/src/ApiServer.cpp:193-209`).
- [ ] Encapsular a execução em shell com `trap` de `EXIT`, `INT` e `TERM`, guardando o PID/grupo de processos do `xvfb-run`/`LightpackTests` e encerrando-o com `TERM`, seguido de `KILL` se necessário. Isso garante limpeza tanto em timeout quanto em cancelamento do job e evita deixar a porta `3636` ou um X virtual vivos para passos posteriores.

Trecho ilustrativo do passo de execução (a instalação e a compilação ficam em passos anteriores):

```sh
ss -ltn 'sport = :3636' | grep -q ':3636' && { echo 'porta 3636 ocupada'; exit 1; }
setsid xvfb-run -a env QT_QPA_PLATFORM=offscreen ./tests/bin/LightpackTests > test-output.log 2>&1 &
test_pid=$!
(
  sleep 120
  kill -TERM -- -"$test_pid" 2>/dev/null || true
  sleep 10
  kill -KILL -- -"$test_pid" 2>/dev/null || true
) &
watchdog_pid=$!
trap 'kill "$watchdog_pid" 2>/dev/null || true; kill -TERM -- -"$test_pid" 2>/dev/null || true; wait "$test_pid" 2>/dev/null || true' EXIT INT TERM
wait "$test_pid"; test_status=$?
kill "$watchdog_pid" 2>/dev/null || true
wait "$watchdog_pid" 2>/dev/null || true
exit "$test_status"
```

O código final deve preferir um pequeno wrapper versionado ou um timeout equivalente que preserve o PID conhecido; o objetivo é manter o `trap` acima efetivo e retornar o exit code original de `LightpackTests`.

### Fase 2 — Check de versionamento (`RELEASE_VERSION` vs `VERSION_STR`)

- [ ] Criar um script POSIX versionado, por exemplo `scripts/ci/check-release-version.sh`, que leia `Software/RELEASE_VERSION` sem espaços extras e extraia exclusivamente o literal de `#define VERSION_STR "..."` de `Software/src/version.h`; ele deve falhar com ambos os valores e instrução de correção quando estiverem ausentes, ambíguos ou diferentes.
- [ ] Executar esse script em um job inicial `verify-release-version` no GitHub Actions e declarar `needs: verify-release-version` nos jobs de build/teste, de modo que uma divergência bloqueie os builds e artefatos, não apenas marque um passo tardio como falho.
- [ ] Manter `Software/RELEASE_VERSION` como fonte operacional do empacotamento por enquanto: os builds `dpkg` e `flatpak` o leem diretamente (`Software/dist_linux/dpkg/build.sh:10`; `Software/dist_linux/flatpak/build.sh:5`) e o job macOS o usa no nome do artefato (`.github/workflows/ci.yml:237-244`). O check reduz o risco sem introduzir, nesta mudança, geração automática de header.
- [ ] Validar a comparação contra o valor atual, que é `5.11.2.31` em `Software/RELEASE_VERSION:1` e em `Software/src/version.h:30`, e fazer o script rejeitar newline/CRLF indevidos ou mais de uma definição de `VERSION_STR`, caso afetem a extração.

### Fase 3 — Decisão Travis/GitHub Actions

- [ ] **Recomendação: aposentar `.travis.yml` e migrar, somente após validação de propriedade e credenciais, o upload de macOS necessário para o job `build-macos` do GitHub Actions.** GitHub Actions já executa `qmake -r`, `make` e `macdeployqt ... -dmg` (`.github/workflows/ci.yml:231-236`), duplicando o papel de build do Travis (`.travis.yml:20-26`); manter dois CI para o mesmo alvo aumenta manutenção e oferece sinais de build potencialmente divergentes.
- [ ] Inventariar o upload legado antes da migração: ele tenta enviar a DMG a `https://psieg.de/lightpack/osx_builds/...` com secrets `PSIEG_UPLOAD_USER`/`PSIEG_UPLOAD_PASSWORD`, somente em `master` fora de PR (`.travis.yml:33`). Confirmar com o proprietário do endpoint se ele segue ativo, quem controla as credenciais e qual política de retenção/acesso deve vigorar; não copiar secrets possivelmente mortos para GitHub Actions.
- [ ] Corrigir ou eliminar explicitamente a dependência quebrada `cat Software/VERSION` usada pelo Travis (`.travis.yml:17`): esse arquivo não existe no repositório, enquanto o versionamento ativo é `Software/RELEASE_VERSION:1`. A remoção do Travis elimina essa fonte adicional e hoje inválida de divergência.
- [ ] Após a validação do endpoint, adicionar ao `build-macos` um passo de upload protegido por branch/tag e GitHub Secrets, com falha explícita e logs sem segredos; manter o upload de artefato do Actions como trilha de diagnóstico (`.github/workflows/ci.yml:245-249`). Se o endpoint não for mais aprovado, substituir o upload por um destino acordado antes de remover o Travis.
- [ ] Remover `.travis.yml`, desativar o repositório no Travis e atualizar badges/documentação somente quando o novo upload tiver passado pela validação de produção. Não acrescentar criação/publicação automática de GitHub Release a este escopo: não há automação de release no workflow atual e a decisão de política de release é separada da consolidação do build.

## 2. Testes

| Fase | Validação antes do merge | Critério de aceite |
|---|---|---|
| 1 — testes headless | Reproduzir o job em um container/VM Ubuntu equivalente ao runner (por exemplo, `act` com imagem que contenha Docker e as dependências, ou script de CI executado localmente) e executar `LightpackTests` com `QT_QPA_PLATFORM=offscreen` e `xvfb-run -a`. Rodar uma vez com a porta `3636` ocupada por um listener temporário e outra vez induzindo travamento para exercitar o timeout/`trap`. | Execução normal termina com exit code 0; porta ocupada falha antes dos testes com mensagem clara; timeout/cancelamento encerra o grupo de processos e deixa `3636` livre. O run hospedado no GitHub Actions confirma o mesmo comportamento no runner real. |
| 2 — versionamento | Rodar `scripts/ci/check-release-version.sh` no checkout limpo; em um branch temporário, alterar apenas `Software/RELEASE_VERSION` e, em outra tentativa, apenas `VERSION_STR`. Rodar o workflow/pipeline localmente quando possível e abrir um run de teste descartável no GitHub Actions. | O checkout atual passa; cada divergência e cada definição ausente/duplicada falha antes dos jobs dependentes, exibindo os dois valores e sem produzir artefatos. |
| 3 — Travis/Actions | Primeiro executar manualmente o upload a partir de um ambiente de teste autorizado, usando credenciais novas/confirmadas e uma DMG identificável; em seguida, disparar o workflow em branch/tag de teste e confirmar as guardas de publicação, o nome do arquivo e o destino. Verificar que PRs, branches não autorizadas e forks não recebem secrets nem tentam upload. | O artefato chega ao destino aprovado exatamente uma vez pelo Actions; logs não expõem credenciais; o job macOS continua produzindo e anexando a DMG. Só então remover o Travis e confirmar que os checks/badges do repositório não dependem dele. |
