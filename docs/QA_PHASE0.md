# QA — Fase 0

TaskSpec: `pc2ps3-phase0-ambiente`  
ClickUp: `86e3xtgxh` — lista `901718054860`  
Execução: Windows, Python 3.14.7; Docker CLI 29.9.2 presente, daemon `desktop-linux` indisponível.

## Cenários Gherkin

```gherkin
Funcionalidade: Gates seguros e reproduzíveis da Fase 0

  Cenário: setup exige lock real antes de construir
    Dado que docker/toolchain.lock.json não existe
    Quando executo `python tools/phase0.py setup`
    Então o comando termina com código 1
    E informa que falta o lock, sem tentar construir imagem

  Cenário: build PS3 não prossegue sem lock validado
    Dado que docker/toolchain.lock.json não existe
    Quando executo `python tools/phase0.py build-ps3`
    Então o comando termina com código 1
    E informa que falta o lock

  Cenário: clean não apaga artefatos
    Dado que existem ou podem existir saídas geradas locais
    Quando executo `python tools/phase0.py clean`
    Então o comando termina com código 0
    E declara que nenhum arquivo foi removido

  Cenário: empacotamento bloqueia sem ferramentas verificadas
    Quando executo `python tools/phase0.py package-ps3`
    Então o comando termina com código 1
    E não gera EBOOT.BIN, PARAM.SFO nem PKG

  Cenário: teste host exige CMake disponível
    Quando executo `python tools/phase0.py test`
    Então a execução falha se CMake não estiver instalado
    E não declara aceite completo da Fase 0

  Cenário: Docker precisa de daemon Linux ativo
    Quando consulto `docker info`
    Então a operação falha se o engine Linux não estiver disponível
```

## Evidências executadas

1. `python --version` → `Python 3.14.7` (PASS: runtime presente).
2. `python -m py_compile tools/phase0.py` → código 0 (PASS: sintaxe válida).
3. `python tools/phase0.py --help` → código 0; lista setup/build-ps3/package-ps3/test/clean (PASS: CLI acessível).
4. `python tools/phase0.py setup` → código 1; `ERROR: Missing docker/toolchain.lock.json. See docs/PHASE0.md; no verified image digest is available yet.` (PASS: gate de lock acionado; build real BLOCKED por lock ausente).
5. `python tools/phase0.py build-ps3` → código 1; mesma mensagem de lock ausente (PASS: gate acionado; ELF real BLOCKED).
6. `python tools/phase0.py clean` → código 0; `No files removed. Generated outputs only: build/, build-manual/, dist/, prototype.ppm.` e instrução para inspeção/remoção manual (PASS: nenhuma exclusão; seguro neste estado).
7. `python tools/phase0.py package-ps3` → código 1; `PACKAGING BLOCKED: verified SELF/SFO/PKG utilities and their invocation are unavailable. No EBOOT.BIN, PARAM.SFO or PKG has been generated.` (PASS: bloqueio explícito; PKG real BLOCKED).
8. `python tools/phase0.py test` → código 1; tenta `cmake -S ... -B ...`, Windows `WinError 2` (PASS: falha explícita; testes/build host BLOCKED por CMake ausente).
9. `docker info` → código 1; `open //./pipe/dockerDesktopLinuxEngine: The system cannot find the file specified.` (BLOCKED: daemon Docker Linux não iniciado/disponível).
10. `Get-Command cmake,ninja,g++,ppu-gcc` → sem resultados (BLOCKED: dependências locais ausentes).
11. `docker/toolchain.lock.example.json` contém `null` para imagem, repositórios/commits/licenças/evidências, caminhos PPU e SDL; lock real não existe (BLOCKED: pins imutáveis e SDK não demonstráveis; não foram inventados).

## Situação

Os gates executáveis para lock ausente, limpeza não destrutiva e ausência de empacotadores se comportam como esperado. O ambiente não permite validar build de imagem, ELF PPU, SDL2 ou PKG. A TaskSpec permanece sem aceite integral. O conector ClickUp recusou `get_task` por limite diário (100/100); a UI integrada não pôde ser aberta neste subagente. Por isso os cenários e saídas foram registrados neste relatório local em vez de no card.

## Adendo QA — correção de clean e CMake multi-config

### Cenários Gherkin

```gherkin
Funcionalidade: Teste host em CMake multi-config e limpeza segura

  Cenário: configuração Debug chega ao build e ao CTest
    Dado que o gerador CMake é multi-config
    Quando executo `python tools/phase0.py test`
    Então a configuração Debug é selecionada no build por `--config Debug`
    E o CTest recebe `-C Debug`

  Cenário: clean sem confirmação apenas mostra as saídas
    Quando executo `python tools/phase0.py clean` sem `--confirm`
    Então nenhum alvo é removido
    E a saída informa dry-run

  Cenário: clean destrutivo valida containment antes da remoção
    Dado que existe um dos alvos permitidos de limpeza
    Quando executo clean com `--confirm`
    Então cada alvo e descendente é validado antes de qualquer remoção
    E symlinks, junctions/reparse points, caminhos fora da raiz e tipos inesperados são recusados
```

### Execuções

1. `python -m py_compile tools/phase0.py` → código 0 (PASS: sintaxe Python válida).
2. `python tools/phase0.py clean` → saída informa dry-run e zero arquivos removidos; código 0 (PASS).
3. `\.\scripts\clean.ps1` → saída também informa dry-run e zero arquivos removidos; código 0 (PASS).
4. `Get-Command cmake` → sem saída (CMake ausente).
5. `python tools/phase0.py test` → imprime `+ cmake -S ... -B ... -DCMAKE_BUILD_TYPE=Debug`; falha `WinError 2` ao tentar iniciar CMake; código 1 (BLOCKED por CMake ausente). O código fonte inclui `--config Debug` para `cmake --build` e `-C Debug` para CTest; sua passagem end-to-end permanece BLOCKED pelo executável ausente.

### Revisão estática de clean destrutivo (não executado)

A opção destrutiva não foi executada por este QA. A revisão estática confirma uma allowlist contida na raiz do projeto e registra risco de corrida por alterações concorrentes no filesystem.

### Adendo para o card ClickUp

Correção revisada: o comando de teste agora envia `--config Debug` ao `cmake --build` e `-C Debug` ao CTest para suportar geradores multi-config. Com CMake ausente neste host, só foi possível verificar o comando de configuração (inclui `-DCMAKE_BUILD_TYPE=Debug`) e confirmar a falha explícita `WinError 2`; o caminho de build/CTest permanece BLOCKED até CMake estar instalado. `clean` sem `--confirm` foi executado pelo CLI e pelo wrapper PowerShell; ambos terminaram em código 0 e informaram dry-run sem remover saídas. `clean --confirm` não foi executado por instrução explícita; revisão estática verificou allowlist, containment, recusa de symlink/junction/reparse point e validação dos descendentes antes de remover. Há risco TOCTOU documentado para mutações concorrentes no filesystem.

### Revalidação dos gates seguros — 2026-11-02

- `Test-Path docker/toolchain.lock.json` → `False`.
- `python tools/phase0.py setup` → exit code 1, informa lock ausente antes de
  chamar Docker.
- `python tools/phase0.py build-ps3` → exit code 1 pelo mesmo gate, sem executar
  o contêiner.
- `python tools/phase0.py package-ps3` → exit code 1, bloqueia sem ferramentas
  SELF/SFO/PKG verificadas e não gera EBOOT.BIN, PARAM.SFO ou PKG.
- `python tools/phase0.py clean` → exit code 0, lista `build` como saída que
  seria removida e confirma dry-run sem apagar arquivos.
- `--confirm`, build Docker, ELF PPU e PKG real não foram executados.

## Cenário do layout do checkout

```gherkin
Feature: Layout documentado do PC2PS3

  Scenario: Cada responsabilidade tem um caminho definido
    Given o mapa docs/PROJECT_STRUCTURE.md
    When verifico os diretórios e READMEs listados
    Then os caminhos do core, análise, plataforma, jogo original, ferramentas e packaging existem
    And os scripts chamados pelo CMake continuam nos caminhos atuais
    And a documentação marca backend PS3 e PKG como pendentes
```

Execução: `Test-Path` retornou True para `docs/PROJECT_STRUCTURE.md`,
`analysis/asset-inventory/README.md`, `platform/pc/README.md`,
`platform/ps3/README.md`, `games/original/README.md`, `tools/asset/README.md`,
`packaging/README.md`, `include/pc2ps3`, `src/core`, `samples` e `docker`. `rg`
confirmou que CMake continua usando `src/core/` e
`tools/asset_manifest.py`. PASS para a estrutura adaptada; implementação do
backend PS3 e packaging não está coberta por esse aceite.
