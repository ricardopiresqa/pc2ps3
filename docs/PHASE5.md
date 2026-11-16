# Fase 5 — expansão genérica de campanha

Esta fase adiciona primitivas reutilizáveis do framework. Elas não incluem
assets comerciais nem afirmam reproduzir conteúdo específico de jogos comerciais.

## Menu flow

`MenuFlow` modela menu principal, pausa, configurações e confirmação de saída.
O Game Adapter fornece ações e executa os comandos retornados; renderização,
encerramento do processo e transições de cena permanecem fora do controlador.

## Progressão

`CampaignProgression` mantém até 64 fases em máscaras compactas: a primeira
começa desbloqueada e concluir uma fase desbloqueia a seguinte. Um payload
versionado em little-endian é persistido pelo `SaveManager`; progresso inválido
não substitui o estado em memória.

## Chefes

`BossEncounter` controla vida e fases por limiares decrescentes fornecidos pelo
Game Adapter. Padrões de ataque, hitboxes, animações, assets e balanceamento
continuam caller-owned e precisam de especificação por jogo.

Os cenários Gherkin e testes estão em `QA_PHASE5.md`. Em 2026-11-03, CMake
configurou o projeto e compilou os três alvos CTest; `menu_flow`,
`campaign_progression` e `boss_encounter` passaram (3/3) no host MSVC 19.44.
LLVM 19 `clang-tidy` aceitou sintaticamente os três módulos e testes com
`-std=c++11`, mas não linkou nem executou nesse modo. CTest estrito C++11 e
toolchain/hardware PS3 continuam pendentes.
