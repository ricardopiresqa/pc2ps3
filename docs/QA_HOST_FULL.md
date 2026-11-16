# QA geral do host — 2026-11-03

## Ambiente e execução

- CMake com gerador Visual Studio 17 2022, Windows SDK 10.0.26100.0 e MSVC 19.44.35228.0.
- CMake detectou Python 3.14.7.
- Configuração: `cmake -S . -B build/phase4-validation -G "Visual Studio 17 2022" -A x64`.
- Build completo Debug: `cmake --build build/phase4-validation --config Debug --parallel 1 --verbose` — exit code 0.
- Suíte completa: `ctest --test-dir build/phase4-validation -C Debug --output-on-failure` — **69/69 passaram**, 0 falhas, 7,14 s.

A primeira tentativa de build completo com `--parallel 6` terminou com exit code 1 e sem diagnóstico capturado. A compilação sequencial foi concluída e a suíte inteira passou. Os três alvos da Fase 4, três da Fase 5 e quatro da Fase 6C também passaram nos CTests focados.

## Alcance e limites

O resultado cobre os testes do runtime host, adaptadores PC, fluxo de campanha/combate, ferramentas de inventário e pacote de assets, parsers UnityFS/SerializedFile e CLIs locais. A compilação MSVC não declara `LanguageStandard`; CMake solicita C++11, mas a execução CTest não prova runtime estrito C++11. LLVM 19 `clang-tidy` analisou sintaticamente todos os 105 arquivos `.cpp` em `src`, `apps`, `tests` e `platform` com `-std=c++11`, sem linkar ou executar nesse modo.

Não houve build PPU/PS3, SELF/EBOOT.BIN, PKG, instalação no RPCS3 ou teste em hardware. Docker não alcançou o daemon local, WSL retornou `E_ACCESSDENIED` e não há `ppu-gcc` instalado. A documentação/package manifest do REA local foi bloqueada pela avaliação Thoth `SA-35d6fa8597cf053f7d17747c7cc6807e2fcf3ed4` por `PROMPT_INJECTION`; nenhum código desse pacote foi executado.
