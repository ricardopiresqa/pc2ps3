# ADR 0017. Primitivas de armas, padrões por fase e cooperação local

Data: 2026-11-02 · Status: aceita para implementação local

## Contexto

A investigação da Fase 6 cita sistemas de armas, padrões de chefe e cooperação
local. O core ainda não tem essas APIs. `ProjectilePattern`, `BossEncounter` e
`InputManager` já fornecem primitivas que devem ser reutilizadas, mas não há
dados verificados de comportamento específico de Cuphead.

## Decisão

- `WeaponSystem` seleciona perfis emprestados e emite apenas eventos de disparo
  quando o cooldown caller-provided permite. Dano, munição e projéteis ficam no
  adapter.
- `BossPattern` seleciona uma sequência caller-authored por ID de fase e emite
  IDs de ação por tick; o adapter pode encaminhá-los aos sistemas existentes.
- `CoopInputRouter` mantém dois slots independentes de `InputFrame` já mapeados.
  Leitura de dispositivos e associação física dos controles ficam no adapter.
- Cada módulo permanece bounded, sem alocação por tick e sem alterar os módulos
  existentes de input, boss ou projéteis.

## Alternativas consideradas

- Codificar nomes, dano, munição, padrões, input ou atributos de Cuphead foi
  descartado por falta de medições e especificação reproduzível.
- Alterar `InputManager`, `BossEncounter` ou `ProjectileSystem` para incluir
  regras dos adapters foi descartado para preservar seus contratos atuais.
- Um subsistema completo de rede/online foi descartado; o prompt pede
  cooperação local e o escopo fornece apenas roteamento de frames locais.

## Consequências

- Os adapters continuam responsáveis por dispositivos, saúde, munição, AI,
  execução de ações e apresentação.
- Os perfis e sequências são emprestados e devem sobreviver aos componentes.
- Os testes manuais em MSVC C++14 não substituem CTest nem provam C++11.
