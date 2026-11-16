# ADR 0018. Marcadores de animação, co-op local e integração Fase 6

Data: 2026-11-02 · Status: aceita para implementação local

## Contexto

A Fase 6 pede suporte a grande quantidade de animações/efeitos, cooperação local
e integração entre combate, armas, parry, projéteis e fases de chefe. O projeto já
possui `AnimationPlayer`, `CoopInputRouter`, `World` e as primitivas da Fase 6,
mas ainda faltava uma saída explícita de eventos de frame e um runner local que
evitasse avanço parcial de dois mundos.

## Decisão

- `AnimationFrame` recebe um marcador opcional (`event_id == 0` significa nenhum).
  `AnimationPlayer::update_with_events` emite os markers atravessados num buffer
  caller-owned, após validar capacidade, e limita transições por chamada. Clips
  com markers usam essa API para não perder eventos.
- `LocalCoopSession` referencia dois `World`s distintos e um
  `CoopInputRouter`; valida ambos os frames antes de avançar qualquer mundo.
- Um demo headless liga armas, agenda de projéteis, contatos, dano, parry e fases
  de chefe com IDs/valores sintéticos.
- Todos os módulos continuam sem assets ou regras específicas de Cuphead; o
  runner coop atual não implementa colisão entre jogadores nem mundo compartilhado.

## Alternativas consideradas

- Callbacks de frame foram descartados para evitar reentrância e ownership
  implícito; o chamador fornece armazenamento para os eventos.
- Converter `World` em simulação multi-actor compartilhada foi adiado; exigiria
  mudar colisões e estado do runtime, além do escopo desta primitive.
- Dados/assets específicos de Cuphead foram adiados por falta de instalação
  autorizada e evidência reproduzível.

## Consequências

- `update_with_events` falha sem mutação se o buffer for pequeno ou a chamada
  cruzar transições demais; a API legada continua disponível para clips sem
  markers.
- Para um combate coop em mundo compartilhado, o adapter ainda precisa integrar
  os atores em uma única simulação.
- O demo prova wiring host com valores sintéticos, não fidelity do jogo nem
  execução em PS3.
