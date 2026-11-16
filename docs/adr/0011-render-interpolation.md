# ADR 0011 — Interpolação visual entre ticks fixos

- Data: 2026-11-02
- Status: aceita para implementação local
- TaskSpecs: `pc2ps3-phase2-render-alpha`, `pc2ps3-phase2-world-interpolation`, `pc2ps3-phase2-headless-runtime-demo`

## Contexto

O `FixedStepScheduler` já calcula `alpha` como a fração restante do timestep, mas o contrato de render ainda não a expõe. `World` oferece somente o estado atual do ator, e os demos headless executam loops próprios. A interpolação deve melhorar a apresentação sem contaminar a simulação determinística.

## Decisão

1. `RuntimeApplication::render` recebe o `alpha` calculado para o frame atual. O runtime continua chamando render e present uma vez por frame, inclusive quando nenhum update ocorreu.
2. `World` guarda apenas a posição anterior do player para consulta visual. A consulta interpola linearmente posição anterior → posição atual para alpha finito em `[0,1)` e falha sem alterar a saída para alpha inválido.
3. A posição interpolada não substitui `Actor::position`, não afeta colisões, velocidade, grounded, tick, input nem o próximo update. `reset()` sincroniza o snapshot anterior à posição inicial.
4. Um demo headless scripted compõe `RuntimeLoop`, `World` e `Framebuffer` sem relógio de parede, backend de dispositivo ou assets comerciais. Ele demonstra input, catch-up, frame sem tick, interpolação e quit normal.
5. O host calcula elapsed e fornece snapshots. O loop permanece single-threaded; alpha não é pacing e a interpolação não garante ausência de clipping em colisões discretas.

## Alternativas descartadas

- Passar elapsed ou relógio ao `World`: acoplaria a simulação ao host.
- Alterar a posição física para renderizar um estado interpolado: poderia contaminar colisão e próximo tick.
- Fazer o demo scripted representar SDL2/PSL1GHT: seria evidência falsa de integração nativa.

## Consequências e rollback

Renderizadores recebem um valor de alpha uniforme por frame, e a consulta visual do `World` não aloca. O snapshot guarda somente posição do player; outras entidades podem adotar o mesmo contrato em tarefa própria. Rollback: reverter os três cards desta decisão, mantendo o scheduler, o loop e a física existentes.
