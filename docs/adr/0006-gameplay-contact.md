# 0006. Gameplay cinemático e contatos de combate

Data: 2026-11-01 · Status: aceita

## Contexto

A Fase 2 pede plataformas móveis, colisões de hitbox/hurtbox e knockback. Esses
recursos precisam continuar portáveis e reutilizar o `World`/AABB existentes,
sem presumir parâmetros de jogos comerciais.

## Decisão

Plataformas cinemáticas entram no `World` com velocidade fornecida pelo jogo e
movimento pelo `fixed_dt`; o jogador apoiado recebe o mesmo delta. O
`HitboxSystem` recebe as caixas válidas de cada tick e emite pares sobrepostos
em ordem estável, sem aplicar dano. Knockback é um impulso de velocidade finito
fornecido pelo chamador e integrado ao `World`.

## Alternativas consideradas

- Criar rotas, dano, stun e valores padrão para os jogos: adiado por depender de
  comportamento medido e específico do jogo.
- Usar colisão contínua: adiada; o runtime atual usa consultas discretas por
  tick e não há cenário de medida que justifique mudar o resolvedor nesta tarefa.
- Callbacks para processar hits: rejeitado para manter o módulo sem reentrância
  e com eventos explícitos.

## Consequências

O chamador controla caixas por tick, debounce, dano, stun e velocidade. Caixas e
plataformas móveis ainda usam AABB discreto, portanto objetos rápidos podem
atravessar volumes entre ticks. A movimentação depende do `fixed_dt` do World.
