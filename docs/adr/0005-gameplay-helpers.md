# 0005. Helpers de gameplay limitados e sem callbacks

Data: 2026-11-01 · Status: aceita

## Contexto

A Fase 2 precisa de uma máquina de estados, checkpoints e projéteis sem acoplar o
runtime a renderer, plataforma, conteúdo comercial ou regras específicas de um
jogo. O host disponível permite compilar e executar testes manuais, enquanto
CMake/CTest e o compilador estrito C++11 permanecem indisponíveis.

## Decisão

Implementar os três recursos como componentes C++11 pequenos e bounded. A
máquina de estados recebe IDs e um grafo de transições explícitas, sem callbacks.
Checkpoints guardam posições de respawn e são ativados pela lógica do jogo a
partir de eventos do `TriggerSystem`. Projéteis usam pool pré-alocado, movimento
Q16.16 e colisão discreta por AABB; dano e ownership ficam no chamador.

## Alternativas consideradas

- Colocar transições, respawn e projéteis diretamente em `World`: rejeitado para
  manter cada regra disponível aos adaptadores sem tornar o `World` específico.
- Adicionar callbacks ou dependência de uma plataforma: rejeitado porque o
  comportamento deve continuar determinístico e testável no host.
- Varredura contínua para projéteis: adiada; já é uma limitação conhecida da
  colisão AABB por tick e exige uma tarefa própria.

## Consequências

Os módulos ficam reutilizáveis e não alocam no caminho quente. Cada jogo ainda
precisa conectar regras de transição, eventos de trigger, dano e persistência.
Projéteis rápidos podem atravessar alvos entre atualizações. Build via CMake,
CTest e validação C++11 estrita ficam pendentes até a toolchain host estar
disponível.
