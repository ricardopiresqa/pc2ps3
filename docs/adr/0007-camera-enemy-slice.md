# 0007. Câmera explícita e vertical slice host

Data: 2026-11-01 · Status: aceita

## Contexto

A Fase 2 precisa de scroll de câmera reutilizável e armazenamento de inimigos
sem comportamento presumido. Uma demo host deve conectar gameplay e renderer
com arte original antes de qualquer adaptação a um jogo comercial.

## Decisão

`Camera2D` centraliza alvos e clampa aos limites do mundo sem smoothing
implícito. `EnemySystem` fornece slots limitados para IDs, bounds e health
configurados pelo jogo, sem AI. `pc2ps3_prototype` integra esses contratos com
movimento, estado, combate, checkpoint e animação sintética.

## Alternativas consideradas

- Adicionar deadzone, smoothing ou AI padrão: rejeitado por introduzir regras de
  jogo sem evidência.
- Usar assets de jogos comerciais: rejeitado; a demo precisa permanecer original e
  independente de arquivos comerciais.
- Conectar diretamente SDL2 ou PSL1GHT: adiado até os backends e toolchain
  estarem verificados.

## Consequências

A cena demonstra integração host e gera uma imagem reproduzível. Movimentação,
AI, dano, áudio e fidelidade continuam pertencendo a adaptadores futuros; o
resultado não comprova comportamento no PS3.
