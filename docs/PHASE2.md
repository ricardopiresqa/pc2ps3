# Fase 2 — runtime 2D

## Recortes implementados

- `pc2ps3::Aabb` representa limites `left`, `top`, `right`, `bottom`; `query_aabb` detecta sobreposição e retorna normal e penetração de menor separação. Caixas inválidas ou degeneradas não colidem, contenção é tratada e bordas apenas encostando não sobrepõem.
- `World::add_solid` registra sólidos AABB estáticos. O jogador mede 24×32, usa `position.x` como centro horizontal e `position.y` como posição dos pés. `World::step` integra com timestep fixo, resolve sobreposições em até quatro passagens, zera velocidade no eixo de contato e marca `grounded` ao pousar em um sólido.
- `InputMapper` combina teclado, D-pad e estados lógicos do DualShock 3. Mapeia Cross para confirmar/pular, Square para ataque, Circle para cancelar, Triangle para habilidade, L1/R1 para ações configuráveis, Start para pausa e Select para menu auxiliar. Cada ação expõe `pressed` e `held`; `reset()` limpa todas as bordas. Eixos assinados preservam movimento analógico em `move_x/move_y`; direções digitais prevalecem sobre o analógico no mesmo eixo e oposição digital resulta em neutro. O `World` usa o eixo horizontal proporcionalmente e ainda aceita `InputFrame` legado com `left/right`. Adaptadores SDL2/PSL1GHT devem normalizar eixos e aplicar dead-zone calibrado antes de preencher `InputSnapshot`; esses adaptadores de hardware continuam pendentes.
- O demo headless alimenta o `InputMapper` com estados simulados de teclado. O núcleo continua independente de SDL2/PSL1GHT.
- `AnimationClip` guarda frames ordenados com retângulos de atlas e duração em segundos. `AnimationPlayer` suporta loop e reprodução one-shot, avanço por `double`, e ignora deltas inválidos ou não positivos. `reset()` preserva o clip selecionado e limpa progresso/conclusão; `play()` reinicia a reprodução. O clip é referenciado como imutável e precisa viver mais que o player.
- `SceneManager` possui as cenas ativa e pendente por `unique_ptr`. Transições são aplicadas em `update()` com ordem de saída, destruição e entrada; pedidos feitos durante `Scene::update` só são aplicados depois do callback. O manager é single-threaded, não copiável/movível e rejeita transições em callbacks de ciclo de vida ou durante reentrância.

## Limites

A resolução AABB é discreta e não usa varredura contínua; sólidos mais finos que o deslocamento de um tick podem ser atravessados. `World::add_solid` continua representando sólidos estáticos; há suporte adicional a plataformas móveis AABB sem pathfinding ou colisão contínua. Ainda falta traduzir eventos SDL2 e estados da API de controles PSL1GHT para `InputSnapshot`. O `AnimationPlayer` seleciona frames e o `Framebuffer` desenha atlas RGBA no host; a apresentação por backend de dispositivo continua pendente. O gerenciador de cenas não integra renderer, plataforma ou filesystem; callbacks devem permanecer `noexcept` e o uso é single-threaded. `FileSystem` e `SaveManager` oferecem contratos portáveis e um envelope de save versionado, mas ainda não há backend de disco/PS3 nem evidência de atomicidade no hardware. Sensores, máquina de estados, checkpoints, projéteis, hitboxes, hurtboxes e knockback têm módulos portáteis no host; sensores, hitboxes e projéteis usam colisão discreta por tick. O mixer de áudio e o HUD já têm contratos portáveis; ainda faltam áudio em dispositivo, coleta de RAM do processo e apresentação por backends PC/PS3.

## Filesystem e saves

`FileSystem` define leitura limitada, gravação atômica e remoção para paths lógicos. O adaptador deve respeitar o limite de leitura antes de alocar o arquivo e deve preservar o save anterior quando `write_atomic` falha. `SaveManager` recebe root confiável, quantidade de slots e limite de payload; o jogo escolhe somente um índice numérico, e os nomes de slot são gerados internamente.

O formato `PC2S` versão 1 armazena versão e comprimento little-endian e CRC-32/IEEE do payload. O teto é 16 MiB por slot. A leitura rejeita arquivo acima do limite, truncamento, bytes extras, versão não suportada e CRC inválido; ela só altera o buffer de saída após validar o registro completo. CRC detecta corrupção acidental, não autenticidade. O contrato e o rollback estão em [ADR 0003](adr/0003-save-storage.md).

## Áudio portátil

`PcmClip` fornece PCM16 mono/stereo até 16 MiB e `decode_pcm_wav()` aceita RIFF/WAVE PCM inteiro 8/16-bit, rejeita arquivos acima de 32 MiB, e valida chunks com limite explícito de frames. `AudioManager` mistura até 64 efeitos com resampling linear para PCM16 estéreo, volume master/categoria e saturação; música chega por `PcmStream` pull já convertido para a taxa de saída. O mixer não aloca nem libera a última referência de clip/stream dentro de `render()`. Ele é single-threaded; o adaptador deve serializar callbacks e comandos.

Não há dispositivo, SDL2, CellAudio ou codec comprimido neste núcleo. O backend host/PS3, conversão de dispositivo, latência real e comportamento audível continuam pendentes; consulte [ADR 0004](adr/0004-audio.md).

## Verificação

Os alvos C++ permanentes foram compilados em Debug com CMake 3.31.6 e MSVC 19.44.35228.0; a suíte host completa passou em 31/31 testes, incluindo física, input, animação, cenas, filesystem/saves, áudio e os adaptadores de plataforma. A saída está em `build/qa/full-suite/Testing/Temporary/LastTest.log`; cenários e limites estão em [QA_PHASE2.md](QA_PHASE2.md). O CTest deve ser executado em Debug porque os testes usam `assert`, que Release desativa via `NDEBUG`. Isso valida apenas o runtime host: backend SDL2/PSL1GHT, toolchain PS3, RPCS3 e hardware seguem pendentes.

## HUD de depuração portátil
`Framebuffer::draw_debug_hud()` desenha FPS e memória em bytes com uma fonte 3×5
escalada, painel opaco e clipping aos limites do framebuffer. O chamador fornece
as métricas: FPS inválido ou negativo aparece como zero, FPS alto é limitado a
9999 e o valor de memória conserva toda a faixa de `uint64_t`. Isso não coleta
memória do processo; medições reais continuam a cargo dos adaptadores SDL2 e
PSL1GHT, ainda pendentes. O alvo `debug_hud` cobre pixels, formatação e clipping;
execução C++ depende de CMake e compilador disponíveis.

## Texturas RGBA e regiões de atlas
`Texture` mantém uma cópia própria de pixels RGBA e rejeita dimensões ou contagens
inconsistentes. `Framebuffer::draw_texture_region()` copia uma região sem escala,
recorta o destino e compõe alpha straight-over de forma determinística. O
`decode_png()` fornece PNG RGBA já convertido para `Texture`, sem subprocesso ou
biblioteca externa. O alvo `render_texture` cobre cópia opaca, transparência,
blend, retângulos inválidos e clipping; `image_decoder` cobre filtros, tipos de
cor, falhas de integridade e os PNGs de fase 0. A apresentação SDL2/PSL1GHT
continua pendente.
`Framebuffer::draw_texture_region_scaled()` amplia e reduz a região com
nearest-neighbor por divisão inteira, clipping do destino e a mesma composição
alpha straight-over do desenho 1:1. Regiões inválidas falham antes da escrita;
retângulos totalmente fora da viewport são tratados como sucesso sem desenho.
`Framebuffer::draw_texture_region_rotated()` aceita graus inteiros de 0 a 359,
usa o centro geométrico como pivô e nearest-neighbor no inverse mapping; ângulos
cardinais têm mapeamento exato. Rotação é visual, não altera simulação e não
valida desempenho de backend RSX.
`RenderBatch` reserva uma capacidade fixa de até 4096 comandos, preserva ordem
de submissão e pré-valida antes do flush. As `Texture` são referências
emprestadas até `flush()` ou `clear()`. O lote chama o `Framebuffer` host e não
representa batching ou aceleração GPU/RSX.

## Renderização do frame animado
`Framebuffer::draw_animation_frame()` lê apenas o frame atual de `AnimationPlayer` e
delega ao desenho de região da `Texture`. A chamada não avança o tempo. O player,
o clip referenciado por ele e a textura continuam pertencendo ao chamador e
devem permanecer vivos durante o uso. O alvo `animation_render` cobre troca de
frame, clip concluído, player sem frame e retângulo fora da textura.

O demo headless alimenta o InputMapper com estados simulados, gera um atlas de sprite original em memória, anima e desenha o frame selecionado e mostra o HUD no PPM. A taxa exibida é o alvo do timestep, e MEM contabiliza somente a capacidade dos buffers de pixels da textura e framebuffer; não é RSS nem memória total do processo. O núcleo continua independente de SDL2/PSL1GHT.

## Decodificador PNG
`decode_png()` aceita PNG 8-bit não interlaçado dos tipos grayscale, RGB,
indexed, gray-alpha e RGBA. Valida assinatura, ordem e CRC dos chunks, zlib,
DEFLATE e Adler-32; implementa filtros PNG 0–4. A entrada é limitada a 32 MiB,
cada eixo a 4096 pixels e a área a 4.194.304 pixels. Adam7, profundidades não
suportadas e chunks críticos desconhecidos são rejeitados. Não há dependência de
zlib nem chamada a subprocesso.

## Tilemap, camadas e scrolling
`TileMap` guarda até 1.048.576 células no total e até 32 camadas, com depth
estável e `-1` para células vazias. `draw_tilemap()` mapeia índices para tiles
uniformes da atlas, aplica scroll da câmera em pixels e desenha apenas a área
visível pelo clipping já existente do framebuffer.

## Partículas
`ParticleSystem` tem até 1024 slots pré-alocados e não escolhe aleatoriedade:
o chamador fornece posição, velocidade, tamanho, cor e lifetime. Atualização
usa milissegundos inteiros e posição Q16.16, lifetime máximo de 600.000 ms,
velocidade limitada a 1.000.000 px/s e desenho reutiliza o clipping/alpha do
framebuffer via `Framebuffer::blend_rect()`; `fill_rect()` mantém a escrita
opaca original.

## Gerenciamento e carregamento sob demanda de texturas
`TextureManager` combina `FileSystem`, `decode_png()` e `Texture`: o primeiro
`load()` de um path lógico relativo lê e decodifica PNG; cargas repetidas do
mesmo path compartilham o slot e incrementam referências. Cada carga bem-
sucedida deve ter um `unload()` correspondente. O último `unload()` invalida o
handle e limpa dimensões/pixels lógicos, preservando a capacidade do buffer para
uma textura igual ou menor. `trim_unused_buffers()` libera buffers ociosos e o
scratch de bytes PNG.
Cada chamada bem-sucedida recebe um identificador de lease próprio; handles
copiados são aliases da mesma lease, e uma lease liberada não pode ser liberada
novamente nem manter acesso à textura.
Cada instância do manager recebe uma identidade distinta, então handles antigos
continuam rejeitados mesmo se outra instância reutilizar o mesmo endereço.

O manager aceita até 256 slots, paths de até 512 bytes, PNG de até 32 MiB e
limita a 64 MiB a capacidade total dos buffers de pixels retidos. Handles têm
geração e pertencem ao manager que os criou. O cache é single-threaded. O
adaptador `FileSystem` ainda é responsável por mapear paths lógicos sob uma raiz
confiável. O pool aqui é CPU; upload/descarregamento de textura em RSX/SDL2
continua pendente.

## Janela de salto
`JumpAssist` mantém jump buffer e coyote time por seis chamadas de `World::step`.
Um `jump_pressed` pode ser consumido ao pousar ou logo após sair do chão; o
consumo é único e `reset()` descarta ambas as janelas. A duração é por ticks,
portanto segue o timestep fixo configurado. Isso não altera colisão AABB nem
adiciona colisão contínua.

## Sensores e triggers
`TriggerSystem` registra até 1024 sensores AABB estáticos com IDs únicos. Emite
eventos `enter` e `exit` uma vez por transição, preservando a ordem de registro;
o armazenamento dos eventos é reservado ao construir o sistema, sem alocação em
`update()`. O sistema é single-threaded e discreto por tick; não varre movimento
entre ticks nem executa callbacks.

## Máquina de estados
`StateMachine` mantém um grafo explícito de até 64 estados. O chamador declara
transições permitidas; IDs inválidos e arestas não declaradas são rejeitados sem
mudar o estado atual. Repetir o estado atual é uma operação idempotente. O
módulo não chama callbacks nem decide regras de gameplay; o jogo define seus
estados e escolhe quando transicionar.

## Checkpoints
`CheckpointSystem` guarda até 128 IDs e posições de respawn finitas. O jogo
ativa um ID em resposta a um `enter` do `TriggerSystem`; respawn devolve o ponto
ativo ou a posição inicial. `reset()` desativa o checkpoint mantendo os pontos
registrados. A persistência entre sessões continua sendo responsabilidade do
`SaveManager`.

## Projéteis
`ProjectileSystem` mantém até 1024 projéteis pré-alocados, com IDs únicos entre
os ativos, dimensões de 1 a 64 pixels, coordenadas até ±1.000.000, velocidade
inteira até 1.000.000 pixels/s e lifetime até 600.000 ms. Cada update aceita até
1.000 ms e usa posição Q16.16 sem alocar; projéteis que ultrapassam o limite de
coordenadas expiram. Colisões consultam o AABB atual, consomem os projéteis
atingidos e emitem IDs em ordem dos slots. O desenho usa clipping e alpha do
`Framebuffer`. A colisão é discreta, sem sweep contínuo; dano, equipe/owner e
regras de jogo ficam no chamador.

## Plataformas móveis
`World::add_moving_platform()` aceita até 128 AABBs com velocidade finita.
Cada `World::step()` move as plataformas pelo `fixed_dt`; o jogador apoiado é
transportado junto e continua usando a resolução AABB existente. `reset()`
restaura as posições iniciais. Não há rotas, limites de percurso, colisão entre
plataformas ou sweep contínuo.

## Hitboxes e hurtboxes
`HitboxSystem` mantém capacidades separadas de até 128 caixas por tipo. O jogo
registra as caixas do tick, e `update()` emite os pares AABB sobrepostos em
ordem de registro, ignorando caixas com o mesmo owner. Cada par volta a ser
emitido em atualizações seguintes enquanto houver sobreposição; debounce por
ataque e aplicação de dano pertencem ao chamador.

## Knockback
`apply_knockback_impulse()` soma ao `Actor` o impulso de velocidade fornecido
pelo jogo e rejeita valores não finitos ou resultados fora da faixa de `float`
sem alterar o ator. Um impulso vertical remove `grounded`; `World` expõe a mesma
operação para o jogador. Stun, dano e valores específicos dos jogos não são
inferidos pelo framework.

## Camera2D
`Camera2D` mantém scroll integer para um viewport configurável até 4096×4096.
`follow()` centraliza o alvo, limita o scroll aos limites finitos do mundo e
centraliza mapas menores que o viewport. Não há smoothing ou deadzone implícitos;
o chamador decide quando atualizar a câmera.

## EnemySystem
`EnemySystem` oferece até 128 slots com ID, AABB e health fornecidos pelo jogo.
O jogo atualiza bounds e aplica dano usando eventos de combate. A derrota libera
o slot; spawn reutiliza o primeiro slot livre. O módulo não implementa AI,
movimento, dano padrão nem comportamento extraído dos jogos comerciais.
## Runtime portátil e agendamento de frames
O lote implementado adiciona três contratos C++11 ao core. `FixedStepScheduler` transforma elapsed em nanossegundos em ticks limitados, informa dropped time e alpha; `InputTickDistributor` retém held/eixos atuais e entrega flags pressed uma vez no próximo tick; `RuntimeLoop` conecta polling abstrato, input, updates fixos, render e apresentação, com status explícito e cleanup determinístico. A arquitetura está em [ADR 0010](adr/0010-runtime-loop.md), e os cenários/execução estão em [QA_PHASE2.md](QA_PHASE2.md). Não há pacing, backend SDL2/PSL1GHT, leitura de dispositivo, áudio nativo ou validação em hardware PS3.
## Interpolação visual entre ticks
O RuntimeLoop passa o alpha do FixedStepScheduler ao callback de render. World guarda a posição anterior do player para consulta visual interpolada; física e colisões continuam usando exclusivamente Actor::position. Um demo scripted headless compõe RuntimeLoop, World e Framebuffer sem relógio ou backend nativo. A ADR 0011 define o contrato; QA e resultado estão em [QA_PHASE2.md](QA_PHASE2.md), com cards em `LOCAL_BACKLOG.md` (nota local, fora do Git).

## Texto ASCII 3x5 no framebuffer

`Framebuffer::draw_text()` recebe ponteiro e comprimento explicito, aceita ate
1024 bytes e escala de 1 a 8 sem alocar. A fonte cobre A-Z, 0-9 e os sinais
`!`, `%`, `:`, `.`, `?`, `-`, `/` e `_`; letras minusculas viram
maiusculas e bytes sem glifo usam `?`. Espaco, tab, retorno de carro e nova
linha controlam o cursor; os pixels sao recortados pelo framebuffer. O HUD de
depuracao desenha pela mesma API e pela mesma tabela de glifos.

## Overlay RGBA para fade e flash

`Framebuffer::blend_overlay()` compoe a cor sobre todos os pixels com a mesma
funcao straight-over de `blend_rect()`. O chamador fornece o alpha de cada
frame para animar fade ou flash; o caminho nao aloca e nao usa shader. A
apresentacao por SDL2/PSL1GHT e RSX permanece pendente.

## Metricas de duracao de frame

`FrameTimeTracker` mantem as ultimas 120 duracoes validas de 1 a 10.000.000.000
ns em armazenamento fixo. Expoe contagem, ultima amostra, media inteira, minimo
e maximo; janela vazia e `reset()` retornam metricas zeradas. O host fornece as
duracoes: o core nao consulta relogio e nao altera o scheduler.
