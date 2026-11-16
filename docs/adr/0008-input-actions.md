# ADR 0008 — Contrato lógico de controles

## Decisão

`InputSnapshot` carrega estados lógicos de teclado/D-pad e botões do DualShock 3,
além de eixos assinados de 16 bits. Adaptadores normalizam a orientação e a
faixa do dispositivo e aplicam dead-zone calibrado antes de preencher o
snapshot. O núcleo não inventa uma dead-zone universal.

`InputMapper` converte esses estados em eixos `move_x/move_y` limitados a
`[-1, 1]` e ações com `pressed` e `held`. Direção digital ativa prevalece sobre
o analógico no mesmo eixo; direções digitais opostas se anulam. Cross também
preserva o contrato anterior de pulo; Square, Circle, Triangle, L1, R1, Start e
Select ficam expostos como ações lógicas para o Game Adapter.

`World` usa apenas `move_x` para acelerar o jogador horizontalmente, mantendo
os campos `left/right` como compatibilidade com frames criados por clientes
anteriores. O movimento vertical permanece disponível ao Game Adapter.

## Motivo

O mapeamento oferece uma fronteira comum para teclado, SDL2 e `cellPad`, mantém
comportamento determinístico entre fontes e deixa calibração dependente do
hardware fora do runtime portátil.

## Consequências

- O layout de `InputFrame` cresce; os campos antigos permanecem válidos.
- Nenhum backend SDL2/PSL1GHT foi incluído nesta decisão.
- A validação C++ do novo alvo depende de compilador e CMake disponíveis.
