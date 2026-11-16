# QA — Fase 2: física AABB

Escopo: `include/pc2ps3/physics.hpp`, `src/core/physics.cpp`, `tests/physics_test.cpp` e `CMakeLists.txt`.

## Cenários mínimos — state machine, checkpoints e projéteis

Os cenários foram executados pela Tifa por executáveis host focados com MSVC
14.44 (`/std:c++14`). O código usa apenas recursos de C++11. CMake/CTest e
compilação estrita `/std:c++11` continuam pendentes no host.

### Máquina de estados

```gherkin
Feature: Máquina de estados determinística

  Scenario: Transição declarada e retorno ao estado inicial
    Given uma máquina válida com estado inicial 0 e a transição 0 para 1
    When o chamador transiciona para o estado 1 e depois chama reset
    Then o estado passa a 1 e reset restaura o estado 0

  Scenario: Transição ausente ou ID inválido
    Given uma máquina válida no estado 0 sem transição para 1
    When o chamador solicita estado 1 ou um ID fora do limite
    Then a operação falha e o estado continua 0
```

### Checkpoints

```gherkin
Feature: Posição de respawn

  Scenario: Trigger enter ativa um checkpoint
    Given um checkpoint de ID 10 e um TriggerSystem com o mesmo ID
    When o ator entra na área e o chamador ativa o checkpoint pelo evento enter
    Then respawn_position devolve a posição registrada para o ID 10

  Scenario: ID inválido e reset preservam o contrato
    Given um checkpoint ativo e uma posição inicial
    When o chamador ativa um ID desconhecido e depois executa reset
    Then o ID desconhecido não altera o ponto ativo e reset retorna à posição inicial
```

### Projéteis

```gherkin
Feature: Pool determinístico de projéteis

  Scenario: Movimento e colisões múltiplas
    Given dois projéteis ativos de IDs 10 e 11 com velocidade de 100 pixels por segundo
    When o sistema atualiza 500 ms e consulta um AABB que cobre as posições novas
    Then os hits contêm IDs 10 e 11 na ordem dos slots e os projéteis são consumidos

  Scenario: Input inválido, expiração e clipping
    Given um projétil de lifetime 10 ms perto da borda do framebuffer
    When dt inválido ou AABB inválido é passado, depois ocorre update e desenho válido
    Then erro não altera estado; após 10 ms o projétil expira e desenho válido é clipped
```

### Plataformas móveis

```gherkin
Feature: Plataforma móvel integrada ao World

  Scenario: Carregar o jogador e restaurar no reset
    Given o jogador apoiado em uma plataforma horizontal de velocidade 2 pixels por segundo
    When World executa um tick e depois reset
    Then o jogador acompanha o deslocamento e o próximo tick parte da posição inicial da plataforma

  Scenario: Transporte vertical e rejeição de plataforma inválida
    Given uma plataforma vertical sob o jogador e uma segunda World sem apoio
    When a plataforma move, e outra entrada tem AABB ou velocidade não finita
    Then o jogador apoiado acompanha o eixo vertical, o sem apoio não é carregado e a entrada inválida falha
```

### Hitbox e hurtbox

```gherkin
Feature: Pares de combate AABB

  Scenario: Emitir pares em ordem e ignorar self-hit
    Given hitboxes e hurtboxes registrados em ordem, incluindo um par do mesmo owner
    When o sistema atualiza
    Then emite todos os pares sobrepostos na ordem registrada e ignora o self-hit

  Scenario: Reiniciar o tick e rejeitar registro inválido
    Given um conjunto de caixas válido
    When begin_tick limpa os registros e o chamador adiciona AABB inválido ou ID duplicado
    Then os registros novos válidos funcionam e nenhuma entrada inválida é adicionada
```

### Knockback

```gherkin
Feature: Impulso de knockback

  Scenario: Aplicar impulso ao Actor e ao jogador do World
    Given um Actor e um jogador grounded
    When um impulso finito é aplicado nos eixos horizontal e vertical
    Then a velocidade recebe o delta e o impulso vertical remove grounded

  Scenario: Preservar estado diante de valores inválidos
    Given um Actor com velocidade finita
    When o impulso contém NaN ou a soma excede a faixa de float
    Then a função falha e mantém a velocidade anterior
```

Resultado desta leva: os três executáveis focados, `core_determinism`, o build
integrado completo e o smoke test do demo passaram em MSVC 14.44 (`/std:c++14`).
CMake/CTest e `/std:c++11` estrito não estavam disponíveis. O build integrado
mantém o warning preexistente C4244 em `src/core/audio.cpp:299`.

## Cenários Gherkin

```gherkin
Funcionalidade: Consulta de contato AABB

  Cenário: overlap no eixo X
    Dado que duas caixas válidas se sobrepõem horizontalmente
    E o menor eixo de penetração é X
    Quando consulto o contato AABB
    Então overlapping é verdadeiro
    E a normal aponta da primeira caixa para a segunda no eixo X
    E penetration é o menor deslocamento de separação nesse eixo

  Cenário: overlap no eixo Y
    Dado que duas caixas válidas se sobrepõem verticalmente
    E o menor eixo de penetração é Y
    Quando consulto o contato AABB
    Então overlapping é verdadeiro
    E a normal aponta da primeira caixa para a segunda no eixo Y
    E penetration é o menor deslocamento de separação nesse eixo

  Cenário: contenção de uma caixa dentro da outra
    Dado que uma caixa válida está inteiramente dentro da outra
    Quando consulto o contato AABB
    Então overlapping é verdadeiro
    E a penetração representa o deslocamento necessário para separar as caixas

  Cenário: normal e penetração para sobreposição parcial
    Dado que duas caixas válidas se sobrepõem parcialmente
    Quando consulto o contato AABB
    Então a normal tem sinal coerente com a direção da segunda caixa
    E penetration corresponde à sobreposição mínima entre os eixos

  Cenário: empate entre penetrações dos eixos
    Dado que duas caixas válidas têm penetrações iguais em X e Y
    Quando consulto o contato AABB repetidamente
    Então a escolha de eixo e a normal são determinísticas

  Esquema do Cenário: bordas encostando não se sobrepõem
    Dado que as caixas válidas encostam pela borda <eixo>
    Quando consulto o contato AABB
    Então overlapping é falso
    E normal e penetration são zero
    Exemplos:
      | eixo |
      | X    |
      | Y    |

  Cenário: borda encostando em coordenadas grandes
    Dado que duas caixas válidas encostam em X na coordenada 16777216
    Quando consulto o contato AABB
    Então overlapping é falso
    E normal e penetration são zero

  Cenário: caixa degenerada
    Dado que uma caixa tem largura ou altura zero
    Quando consulto o contato AABB
    Então overlapping é falso
    E normal e penetration são zero

  Cenário: caixa com coordenada NaN
    Dado que uma coordenada de uma caixa é NaN
    Quando consulto o contato AABB
    Então overlapping é falso
    E normal e penetration são zero
```

## Evidência de execução

Tentativa solicitada de configurar apenas o alvo de física:

```powershell
cmake -S . -B build/qa-phase2 -DPC2PS3_BUILD_TESTS=ON -DPC2PS3_BUILD_TOOLS=OFF
cmake --build build/qa-phase2 --target pc2ps3_physics_test
ctest --test-dir build/qa-phase2 -R pc2ps3_physics_test --output-on-failure
```

Resultado observado no primeiro comando: PowerShell reportou `cmake: The term 'cmake' is not recognized as a name of a cmdlet, function, script file, or executable program.` A configuração não iniciou; build do alvo e CTest não foram executados. Os cenários estão **BLOCKED**, sem resultado PASS, pela ausência de CMake no host.

Revisão documental limitada ao contrato fornecido: o cabeçalho define que caixas devem ter coordenadas finitas e dimensões positivas, contato inválido não sobrepõe, normal aponta da primeira caixa para a segunda e bordas em contato não sobrepõem. O teste-fonte contém casos para overlaps X/Y, contenção, empate determinístico, contato em coordenadas grandes, caixa degenerada e NaN. Execução do teste compilado não foi possível neste ambiente.

## Veredito

**BLOCK** — nenhum cenário foi executado porque CMake não está disponível; a existência de casos no arquivo de teste e as expectativas documentadas não substituem execução.

## QA da integração AABB no World

Configuração de referência dos cenários: `WorldConfig{1/60 s, gravity=1500, move_accel=2200, max_speed=280, jump_speed=560, floor_y=1000}`. O ator reinicia em `(64, 1000)`, com meia largura 12 e altura 32.

```gherkin
Funcionalidade: Resolução de colisões AABB pelo World

  Cenário: colisão lateral contra parede
    Dado um World com a configuração de referência
    E uma parede sólida AABB de (90, 900) até (100, 1000)
    E o jogador recebe direita pressionada por 60 frames de 1/60 s
    Quando cada frame avança o World
    Então a posição X do jogador é aproximadamente 78
    E a velocidade X é 0

  Cenário: aterrissagem em plataforma
    Dado um World com a configuração de referência
    E uma plataforma sólida AABB de (90, 950) até (200, 990)
    E o jogador inicialmente está em (64, 1000), fora da plataforma
    Quando avança um frame com direita, jump_pressed e jump_held
    E avança mais 14 frames com direita e sem novo jump_pressed
    E avança frames idle até aterrissar na plataforma
    Então a posição Y do jogador é aproximadamente 950
    E o jogador está grounded

  Cenário: salto após aterrissar
    Dado que o cenário de aterrissagem terminou com o jogador grounded em Y≈950
    E jump_pressed e jump_held são verdadeiros
    Quando o World avança um frame
    Então a posição Y do jogador fica abaixo de 950
    E o jogador deixa de estar grounded
```

### Execução e evidência

Comando de configuração para habilitar somente o conjunto de testes necessário e desabilitar ferramentas auxiliares:

```powershell
cmake -S . -B build/qa-phase2-world -DPC2PS3_BUILD_TESTS=ON -DPC2PS3_BUILD_TOOLS=OFF
```

Saída: `cmake: The term 'cmake' is not recognized as a name of a cmdlet, function, script file, or executable program.` Configuração não iniciou. Verificação do host: `Get-Command cmake,g++,clang++,cl` não encontrou nenhum deles. Portanto não foi possível compilar/rodar `pc2ps3_physics_test`, que contém os três casos de integração World acima. Resultado dos cenários: **BLOCKED**, não PASS; motivo: CMake e compilador C++ ausentes. Nenhum código de teste foi escrito ou alterado.

## QA da TaskSpec `pc2ps3-phase2-input-adapter`

### Cenários Gherkin

```gherkin
Funcionalidade: Mapeamento de teclado e controle em InputFrame

  Cenário: estado de entrada vazio
    Dado um InputMapper recém-criado
    E um InputSnapshot sem botões ativos
    Quando o mapper coleta o snapshot
    Então left, right, jump_pressed e jump_held são falsos

  Cenário: teclado e DS3 produzem o mesmo mapa lógico
    Dado um InputMapper sem salto previamente pressionado
    Quando coleto teclado_right e teclado_jump ativos
    Então right, jump_pressed e jump_held são verdadeiros e left é falso
    Quando coleto em seguida dualshock3_right e dualshock3_jump ativos
    Então o InputFrame lógico é equivalente

  Cenário: jump_pressed só aparece na primeira borda de pressionamento
    Dado que o salto estava solto
    Quando jump passa a ativo e o snapshot é coletado
    Então jump_pressed e jump_held são verdadeiros
    Quando coleto novamente com salto ainda ativo
    Então jump_pressed é falso e jump_held continua verdadeiro

  Cenário: troca de teclado para DS3 sem nova borda
    Dado que teclado_jump está ativo e já produziu jump_pressed
    Quando teclado_jump é solto e dualshock3_jump fica ativo antes da próxima coleta
    Então jump_held permanece verdadeiro
    E jump_pressed é falso

  Cenário: release seguido de repress gera nova borda
    Dado que o salto foi coletado como ativo
    Quando todos os botões de salto são soltos e coletados
    E um botão de salto é pressionado novamente e coletado
    Então jump_pressed e jump_held são verdadeiros

  Cenário: reset permite nova borda para salto mantido
    Dado que um botão de salto está ativo
    E o mapper já coletou o salto pressionado
    Quando reset é chamado e o mesmo snapshot ativo é coletado
    Então jump_pressed e jump_held são verdadeiros

  Cenário: esquerda e direita simultâneas ficam neutras no World
    Dado um World no estado inicial X=64 com configuração fixa de referência
    E um InputFrame com left=true e right=true
    Quando o World avança um frame
    Então a posição X permanece aproximadamente 64
```

### Execução e evidência

Comando tentado para configurar o conjunto CMake que contém `pc2ps3_input_test`:

```powershell
cmake -S . -B build/qa-input -DPC2PS3_BUILD_TESTS=ON -DPC2PS3_BUILD_TOOLS=OFF
```

Saída exata: `cmake: The term 'cmake' is not recognized as a name of a cmdlet, function, script file, or executable program.` Configuração não iniciou. O host continua sem CMake e sem compiladores C++ (`g++`, `clang++`, `cl` não encontrados na verificação anterior). Não foi possível fazer build do alvo nem executar `ctest --test-dir build/qa-input -R input_mapping --output-on-failure`. Todos os cenários desta seção ficam **BLOCKED**, sem PASS. O arquivo de teste existente não foi alterado e nenhum teste de código foi criado.

## QA da TaskSpec `pc2ps3-phase2-animation`

Arquivos revisados: `include/pc2ps3/animation.hpp`, `src/core/animation.cpp`, `tests/animation_test.cpp` e `CMakeLists.txt`.

### Cenários Gherkin

```gherkin
Funcionalidade: Clips e reprodução de animações

  Cenário: clip vazio ou frame inválido é rejeitado
    Dado um clip sem frames ou com coordenadas negativas, dimensões não positivas
    Quando consulto sua validade
    Então o clip é inválido

  Esquema do Cenário: duração de frame inválida
    Dado um frame com duração <duracao>
    Quando construo um clip contendo o frame
    Então o clip é inválido
    Exemplos:
      | duracao |
      | zero |
      | negativa |
      | NaN |
      | infinito |

  Cenário: player sem clip é seguro e play seleciona clip válido
    Dado um player recém-criado
    Então current_frame é nulo e finished é falso
    Quando chamo play com um clip válido
    Então play retorna verdadeiro
    E current_frame aponta para o primeiro frame com índice e progresso zero

Cenário: play inválido preserva o estado do player
Dado um player que terminou um clip válido
Quando chamo play com um clip inválido
Então play retorna falso
E o clip, frame, índice, progresso e finished permanecem inalterados

  Cenário: reset mantém o clip selecionado e volta ao início
    Dado um player cujo clip selecionado avançou ou terminou
    Quando chamo reset
    Então current_frame aponta para o primeiro frame do mesmo clip
    E current_index e elapsed_in_frame são zero
    E finished é falso

  Cenário: one-shot avança nos limites exatos e termina no último frame
    Dado um clip não cíclico com durações 0.25, 0.5 e 0.25 segundos
    Quando avanço 0.25 segundos
    Então estou no segundo frame com progresso zero
    Quando avanço 0.5 segundos
    Então estou no terceiro frame com progresso zero
    Quando avanço 0.25 segundos
    Então permaneço no terceiro frame, com progresso 0.25 e finished verdadeiro
    Quando avanço mais tempo
    Então o estado terminado permanece inalterado

  Cenário: loop percorre frames e processa delta longo
    Dado um clip cíclico com durações 0.25, 0.5 e 0.25 segundos
    Quando avanço 0.25, 0.5 e 0.25 segundos sucessivamente
    Então cada limite muda para o frame seguinte e o último retorna ao primeiro
    Quando avanço 1000000.25 segundos
    Então o player permanece no segundo frame com progresso zero

  Cenário: loop decimal retorna ao início ao completar a duração
    Dado um clip cíclico com frames de duração 0.1 e 0.2 segundos
    Quando avanço 0.1 e depois 0.2 segundos
    Então o player retorna ao primeiro frame com progresso zero

  Esquema do Cenário: delta inválido não altera o estado
    Dado um player em reprodução com estado conhecido
    Quando atualizo com delta <dt>
    Então índice, progresso e finished permanecem inalterados
    Exemplos:
      | dt |
      | zero |
      | negativo |
      | NaN |
      | infinito |

  Cenário: implementação é independente de SDL e PSL1GHT
    Dado o alvo configurado em C++11
    Quando compilo e executo pc2ps3_animation_test
    Então o alvo não requer SDL nem PSL1GHT
```

### Revisão dos casos no teste-fonte

`tests/animation_test.cpp` contém os casos para clip vazio, retângulo inválido, durações zero/negativa/NaN/infinita, delta zero/negativo/NaN/infinito, one-shot com frames de 0.25/0.5/0.25, loop com delta longo, reset mantendo o clip cíclico, loop decimal com deltas 0.1 e 0.2 e rejeição de play inválido sem alterar estado one-shot concluído. O teste também verifica que reset após reprodução/terminação preserva o clip e retorna índice e progresso ao início. A implementação revisada não inclui SDL nem PSL1GHT; a compatibilidade de compilação C++11 ainda depende de execução.

### Evidência de execução

Tentativa de configuração:

```powershell
cmake -S . -B build/qa-phase2-animation -DPC2PS3_BUILD_TESTS=ON -DPC2PS3_BUILD_TOOLS=OFF
```

Saída exata:

```text
cmake:
Line |
   2 |  cmake -S . -B build/qa-phase2-animation -DPC2PS3_BUILD_TESTS=ON -DPC2 …
     |  ~~~~~
     |  The term 'cmake' is not recognized as a name of a cmdlet, function, script file, or executable program.
Check the spelling of the name, or if a path was included, verify that the path is correct and try again.
```

A configuração falhou antes de gerar o build. Por isso, build do alvo `pc2ps3_animation_test` e CTest não foram executados. Resultado: **BLOCKED**, sem PASS.

### Veredito

**BLOCKED** — CMake não está disponível no host; o alvo `pc2ps3_animation_test` não pôde ser compilado nem executado.

## QA da TaskSpec `pc2ps3-phase2-scenes`

### Cenários Gherkin mínimos

```gherkin
Funcionalidade: Ownership e transições adiadas de cenas

  Cenário: pedido inicial entra e atualiza a cena no próximo update
    Dado um manager sem cena ativa
    Quando transfiro a propriedade de uma cena ao solicitar transição
    E chamo update(0)
    Então a cena recebe on_enter antes de update
    E a cena fica ativa

  Cenário: transição solicitada externamente aplica-se antes do update ativo
    Dado uma cena ativa e uma transição solicitada externamente para outra cena
    Quando chamo update(0)
    Então a cena antiga recebe on_exit e é destruída
    E a nova recebe on_enter e depois update

  Cenário: transição solicitada durante update aplica-se após o callback retornar
    Dado que a cena ativa solicita uma cena nova dentro do seu callback update
    Quando o callback update retorna ao manager
    Então a cena antiga recebe on_exit e é destruída
    E a cena nova recebe on_enter depois da destruição da antiga
    E a nova não recebe update até a próxima chamada ao manager

  Cenário: request_transition rejeita destino nulo e destino duplicado
    Dado que uma cena válida já está pendente
    Quando solicito um destino nulo
    Então o pedido retorna falso
    Quando solicito outro destino enquanto a transição continua pendente
    Então o pedido duplicado retorna falso
    E a propriedade da cena rejeitada é destruída sem callbacks de lifecycle
    E o destino originalmente pendente permanece válido

  Cenário: pedidos feitos por callbacks de lifecycle são rejeitados
    Dado uma cena cujos callbacks on_enter, on_exit e destrutor tentam solicitar transições
    Quando o manager executa os callbacks de lifecycle
    Então cada pedido feito durante esses callbacks retorna falso
    E cada destino rejeitado é destruído sem callbacks de lifecycle
    E a ordem exit, destruição antiga, enter da nova é preservada

  Cenário: update reentrante é rejeitado
    Dado que o callback update da cena ativa chama update novamente no manager
    Quando o callback externo ainda está em execução
    Então a chamada reentrante retorna falso
    E a atualização externa continua e aplica a transição pendente ao terminar o callback

  Esquema do Cenário: delta inválido preserva transição pendente
    Dado um manager sem cena ativa e uma cena válida pendente
    Quando chamo update com delta <dt>
    Então update retorna falso sem executar callbacks
    E a cena continua pendente
    Quando chamo update(0)
    Então a transição pendente é aplicada
    Exemplos:
      | dt |
      | negativo |
      | NaN |
      | infinito |

  Cenário: delta zero é aceito
    Dado uma cena ativa e outra cena pendente
    Quando chamo update(0)
    Então update retorna verdadeiro
    E a transição pendente é aplicada
    E a cena ativa recebe update com delta zero

  Cenário: shutdown encerra cena ativa e destrói cena pendente
    Dado um manager com uma cena ativa e outra cena ainda pendente
    Quando o manager é destruído
    Então a cena ativa recebe on_exit uma vez antes de ser destruída
    E a cena pendente que nunca entrou é apenas destruída
    E nenhuma cena pendente recebe on_enter nem on_exit
```

### Evidência de execução

Alvo indicado pela configuração do projeto: `pc2ps3_scene_manager_test`, registrado no CTest como `scene_manager_lifecycle`. Tentativa headless de configuração:

```powershell
cmake -S . -B build/qa-phase2-scenes -DPC2PS3_BUILD_TESTS=ON -DPC2PS3_BUILD_TOOLS=OFF
```

Saída exata:

```text
cmake:
Line |
   2 |  cmake -S . -B build/qa-phase2-scenes -DPC2PS3_BUILD_TESTS=ON -DPC2PS3 …
     |  ~~~~~
     |  The term 'cmake' is not recognized as a name of a cmdlet, function, script file, or executable program.
Check the spelling of the name, or if a path was included, verify that the path is correct and try again.
```

A configuração falhou antes de gerar o build. Portanto não foi possível executar `cmake --build build/qa-phase2-scenes --target pc2ps3_scene_manager_test` nem `ctest --test-dir build/qa-phase2-scenes -R scene_manager_lifecycle --output-on-failure`. Resultado: **BLOCKED**, sem PASS. Nenhum teste ou código de implementação foi escrito ou alterado nesta etapa.

### Veredito

**BLOCKED** — CMake não está disponível no host; a suíte do SceneManager não foi compilada nem executada.
## SaveManager e FileSystem — TaskSpec `pc2ps3-phase2-filesystem-save`

### Cenários mínimos (Gherkin)

```gherkin
Cenário: salvar e carregar payload com slots isolados
  Dado um FileSystem em memória e um SaveManager válido
  Quando salvo um payload e carrego o mesmo índice de slot
  Então recebo os mesmos bytes
  E o path contém apenas o root e o nome numérico gerado pelo manager

Cenário: aceitar payload vazio e limite configurado
  Dado um slot válido e limite de payload conhecido
  Quando salvo payload vazio ou payload exatamente no limite
  Então a leitura retorna sucesso e o conteúdo correspondente

Cenário: rejeitar slot e payload fora dos limites
  Quando uso índice igual à quantidade de slots ou payload maior que o limite
  Então a operação retorna argumento inválido ou tamanho excedido sem gravar

Cenário: limitar leitura antes de alocar arquivo excessivo
  Dado um registro maior que o limite passado a read_all
  Quando o backend recebe a leitura limitada
  Então retorna tamanho excedido sem substituir o buffer de saída

Cenário: rejeitar registro truncado, com bytes extras, magic ou CRC inválidos
  Quando carrego qualquer uma dessas variantes
  Então recebo dado corrompido
  E o buffer de saída anterior permanece inalterado

Cenário: rejeitar versão futura
  Quando o header contém uma versão desconhecida
  Então recebo versão não suportada sem alterar a saída

Cenário: preservar save anterior se escrita atômica falhar
  Dado um slot que já contém um registro válido
  Quando write_atomic falha ao gravar novo conteúdo
  Então o registro anterior permanece byte a byte igual

Cenário: propagar erros de leitura, escrita, remoção e slot ausente
  Quando o backend falha ou o slot não existe
  Então o SaveManager retorna o estado correspondente sem saída parcial
```

### Evidência de execução

O alvo permanente é `pc2ps3_storage_test`, no CTest `storage_and_saves`. A configuração headless prescrita é:

```powershell
cmake -S . -B build/qa-phase2-storage -DPC2PS3_BUILD_TESTS=ON -DPC2PS3_BUILD_TOOLS=OFF
cmake --build build/qa-phase2-storage --target pc2ps3_storage_test
ctest --test-dir build/qa-phase2-storage -R storage_and_saves --output-on-failure
```

**BLOCKED** — tentativa executada: `cmake -S . -B build/qa-phase2-storage -DPC2PS3_BUILD_TESTS=ON -DPC2PS3_BUILD_TOOLS=OFF`. PowerShell reportou: `The term 'cmake' is not recognized as a name of a cmdlet, function, script file, or executable program.` Build e CTest não foram executados; nenhum cenário está PASS. Retomar quando houver toolchain host disponível. A implementação do backend real PS3 e sua prova de atomicidade também permanecem fora deste recorte.

## AudioManager — TaskSpec `pc2ps3-phase2-audio`

### Cenários mínimos (Gherkin)

```gherkin
Cenário: decodificar WAV PCM8 e PCM16 mono/estéreo
  Dado um RIFF/WAVE PCM válido com chunks em ordem ou padding válido
  Quando decodifico com limite de frames suficiente
  Então obtenho PCM16 interleaved com taxa e canais originais
  E PCM8 unsigned converte 0 para -32768, 128 para 0 e 255 para 32512

Cenário: manter clip anterior para WAV inválido ou acima do limite de 32 MiB
  Dado um clip válido previamente carregado
  Quando o WAV está truncado, tem chunk inválido, formato comprimido, passa de 32 MiB ou excede o limite de frames/PCM
  Então o decoder retorna o erro correspondente
  E o clip de saída continua igual

Cenário: misturar voices simultâneas com volume por categoria
  Dado duas vozes ativas em categorias distintas
  Quando renderizo PCM16 estéreo
  Então mono é duplicado em L/R, o ganho master/categoria é aplicado e soma satura em [-32768,32767]

Cenário: resample linear para a taxa de saída e terminar one-shot
  Dado um clip válido com taxa de entrada diferente da saída
  Quando renderizo blocos consecutivos
  Então o cursor avança deterministicamente por interpolação linear
  E a voz termina quando o clip acaba

Cenário: limitar vozes e interromper por handle ou categoria
  Dado o pool cheio
  Quando peço nova voz ou paro uma voz ativa
  Então a nova voz falha sem handle válido e stop libera o slot
  E stop_category interrompe todas as vozes daquela categoria

Cenário: fazer pull de música em blocos e repetir sem loop infinito
  Dado uma fonte PCM16 estéreo parcial e uma fonte vazia
  Quando a música é renderizada com loop
  Então blocos são consumidos e a fonte não vazia reinicia no fim
  E a fonte vazia não trava o callback

Cenário: rejeitar buffers ou parâmetros de volume inválidos
  Quando render usa ponteiro nulo com frames, ou volume é NaN/fora de [0,1]
  Então a operação falha sem escrever fora do buffer ou alterar volume
```

### Evidência de execução

O alvo permanente é `pc2ps3_audio_test`, no CTest `audio_mixing`. A tentativa headless de configuração:

```powershell
cmake -S . -B build/qa-phase2-audio -DPC2PS3_BUILD_TESTS=ON -DPC2PS3_BUILD_TOOLS=OFF
```

**BLOCKED** — PowerShell não reconhece `cmake` (`The term 'cmake' is not recognized as a name of a cmdlet, function, script file, or executable program.`). Build/CTest não foram executados; nenhum cenário está PASS. Não há backend SDL2/CellAudio nesta entrega, portanto não há evidência de saída audível ou latência no dispositivo.

## Asset manifest — TaskSpec `pc2ps3-phase2-asset-pipeline`

### Cenários mínimos (Gherkin)

```gherkin
Cenário: inventariar em ordem determinística
  Dado uma árvore de assets autorizada
  Quando o scanner calcula manifest
  Então retorna caminhos POSIX ordenados com extensão, categoria, tamanho e SHA-256
  E não inclui conteúdo dos arquivos

Cenário: pular links e arquivos especiais
  Dado um symlink, junction ou arquivo especial dentro da raiz
  Quando o scanner percorre a árvore
  Então não segue o alvo e registra o caminho em skipped

Cenário: rejeitar root ou output inseguros
  Quando root não é diretório regular, é link, ou output fica dentro da raiz
  Então o scanner falha sem publicar JSON parcial

Cenário: falhar se a árvore ou arquivo não puder ser lido
  Dado um arquivo que muda durante o hash ou uma pasta sem acesso
  Quando o inventário termina
  Então não produz manifest incompleto

Cenário: replace atômico protege manifest anterior
  Dado um manifest preexistente e erro no replace
  Quando o novo JSON é gravado
  Então o anterior permanece byte a byte igual e o temporário é removido
```

### Evidência de execução

Comando executado:

```powershell
python -m unittest discover -s tests -p test_asset_manifest.py
```

Resultado: **OK — 7 testes executados, 5 passaram e 2 foram pulados** porque o ambiente Windows sandbox não permite criar symlinks. Os testes cobriram hash SHA-256 conhecido, ordem/categoria, write JSON determinístico, root/output inválidos, alteração durante hash e falha de replace preservando a saída anterior. A integração CTest `asset_manifest_inventory` existe, mas não foi configurada porque CMake não está instalado.

## QA — decodificador PNG

Escopo: `include/pc2ps3/image_decoder.hpp`, `src/core/image_decoder.cpp`,
integração com `Texture` e `tests/image_decoder_test.cpp`.

```gherkin
Funcionalidade: Decodificar PNG em Texture RGBA

Cenário: Tipos de cor suportados preservam os canais
Dado um PNG 8-bit válido grayscale, RGB, indexed, gray-alpha ou RGBA
Quando chamo decode_png com seus bytes em memória
Então recebo uma Texture RGBA com dimensões e pixels corretos
E transparência tRNS e alpha explícito são preservados

Cenário: Filtros PNG e chunks IDAT consecutivos
Dado um PNG cujas linhas usam os filtros 0, 1, 2, 3 e 4
E cujos bytes zlib estão divididos em chunks IDAT consecutivos
Quando decodifico o PNG
Então cada pixel reconstruído corresponde aos canais originais

Cenário: Assets gerados pelo projeto
Dado assets/phase0/ICON0.PNG e assets/phase0/PIC1.PNG
Quando decodifico ambos
Então recebo texturas válidas de 320x176 e 1920x1080

Cenário: Dados inválidos e formatos não suportados
Dado um PNG truncado, com CRC ou Adler-32 incorreto, filtro inválido,
DEFLATE malformado, índice de palette inválido ou chunk crítico desconhecido
Quando chamo decode_png
Então o resultado indica erro e não contém textura parcial

Cenário: Limites de segurança
Dado entrada acima de 32 MiB, eixo acima de 4096 ou área acima de 4.194.304 pixels
Quando chamo decode_png
Então a entrada é rejeitada antes da alocação da imagem
```

Execução da Tifa: **PASS** — os cenários foram exercitados por
`build/qa/image_decoder.exe`; MSVC 14.44 compilou o núcleo e a suíte em
`/std:c++14` (o compilador não oferece modo `/std:c++11`). Também passaram os
testes recompilados `render_texture`, `animation_render` e `core`. CMake/CTest
não pôde ser executado porque CMake não está instalado neste ambiente.

## QA — escala de textura

Escopo: `Framebuffer::draw_texture_region_scaled()` em
`include/pc2ps3/render.hpp`, `src/core/render.cpp` e
`tests/render_texture_test.cpp`.

```gherkin
Funcionalidade: Redimensionar uma região RGBA no Framebuffer

Cenário: Ampliar e reduzir por nearest-neighbor
  Dado um atlas 2x2 com cores diferentes em cada pixel
  Quando desenho a região em 4x4 e depois em 1x1
  Então cada bloco ampliado usa a amostra nearest-neighbor esperada
  E a redução seleciona a primeira amostra conforme a regra inteira

Cenário: Clipping e regiões fora da viewport
  Dado um sprite escalado parcialmente fora do framebuffer
  Quando desenho nos quatro limites ou totalmente fora
  Então apenas pixels visíveis mudam e a chamada totalmente fora tem sucesso

Cenário: Alpha e entradas inválidas
  Dado pixels opacos, transparentes e semitransparentes
  Quando desenho com escala válida ou dimensões/região inválidas
  Então a composição mantém a semântica existente e a falha não escreve pixels
```

Execução da Tifa: **PASS** — o alvo `pc2ps3_render_texture_test` compilou em
CMake Debug e CTest `render_texture` passou 1/1. A execução cobriu ampliação,
redução, amostras nearest-neighbor, clipping, região totalmente fora, alpha e
rejeição sem escrita de dimensões/regiões inválidas. MSVC 19.44 não fornece
modo estrito `/std:c++11`; o projeto declara C++11 no CMake.

## QA — rotação de textura

Escopo: `Framebuffer::draw_texture_region_rotated()` em
`include/pc2ps3/render.hpp`, `src/core/render.cpp` e
`tests/render_transform_test.cpp`.

```gherkin
Funcionalidade: Rotacionar regiões RGBA

Cenário: Rotação cardinal preserva pixels
  Dado um atlas 2x2 e uma linha 2x1 com cores distintas
  Quando desenho em 90, 180 e 270 graus
  Então a ordem espacial dos pixels corresponde à rotação horária
  E o centro do retângulo permanece o pivô

Cenário: Ângulo intermediário e clipping
  Dado um sprite 3x3 em uma viewport menor que seus bounds rotacionados
  Quando desenho em 45 graus nos limites da viewport
  Então pixels são amostrados por nearest-neighbor e só a área visível muda

Cenário: Alpha e entradas inválidas
  Dado pixels transparentes e semitransparentes, ângulo fora de 0..359
  Ou região de origem inválida
  Quando desenho a região rotacionada
  Então alpha usa a composição do Framebuffer e a falha não escreve pixels
```

Execução da Tifa: **PASS** — CMake Debug compilou
`pc2ps3_render_transform_test`; CTest `render_transform` passou 1/1. Os alvos
`render_texture` e `render_transform` passaram juntos em 2/2. Os cenários
cobriram 90/180/270 graus, linha não quadrada, clipping em bordas, 45 graus,
alpha e rejeição sem escrita. MSVC não oferece `/std:c++11` estrito e nenhum
backend RSX/PS3 foi exercitado.

## QA — RenderBatch

Escopo: `include/pc2ps3/render_batch.hpp`, `src/core/render_batch.cpp`,
`tests/render_batch_test.cpp` e o alvo `render_batch` no CTest.

```gherkin
Funcionalidade: Agrupar comandos de sprite no Framebuffer

Cenário: Preservar ordem e composição
  Dado um lote com comandos opacos e alfa em destinos sobrepostos
  Quando descarrego o lote
  Então a ordem de submissão é preservada e alpha usa o Framebuffer

Cenário: Agrupar regiões, escala e rotação
  Dado texturas vivas até o flush e comandos válidos dos três tipos
  Quando descarrego o lote
  Então os pixels correspondem aos métodos imediatos do Framebuffer
  E o lote limpa os comandos após sucesso

Cenário: Capacidade, falhas e retry
  Dado um lote limitado e entradas inválidas ou acima da capacidade
  Quando submeto comandos ou tento flush sem framebuffer disponível
  Então a fila não cresce nem é parcialmente limpa
  E um flush posterior válido descarrega todos os comandos
```

Execução da Tifa: **PASS** — CMake Debug compilou
`pc2ps3_render_batch_test`; CTest `render_batch` passou 1/1. O lote completo
dos alvos `render_texture`, `render_transform` e `render_batch` passou 3/3.
Cobertura: ordem, composição alfa, submissões normal/escalada/rotacionada,
capacidade, entrada inválida, flush vazio e retry após framebuffer indisponível.
MSVC não oferece `/std:c++11` estrito; RSX/SDL2 não foram exercitados.

## QA — plataformas móveis

Escopo: `include/pc2ps3/core.hpp`, `src/core/world.cpp` e
`tests/moving_platform_test.cpp`.

```gherkin
Funcionalidade: Atualizar plataformas cinemáticas no World

Cenário: Validar geometria, velocidade e capacidade
  Dado um World com entradas de AABB não finitas ou velocidade inválida
  Quando registro plataformas fora dos limites ou acima da capacidade 128
  Então cada entrada inválida é rejeitada sem alterar as plataformas válidas

Cenário: Transportar o jogador apoiado e preservar a colisão
  Dado um jogador apoiado em uma plataforma móvel horizontal ou vertical
  Quando o World avança um tick fixo
  Então o jogador acompanha o deslocamento da plataforma e continua grounded
  Mas sem apoio o jogador não recebe deslocamento da plataforma

Cenário: Reset e ordem determinística
  Dado dois Worlds com a mesma configuração e plataformas
  Quando ambos avançam os mesmos ticks e depois são reiniciados
  Então produzem as mesmas posições e o reset restaura a posição inicial
```

Execução da Tifa: **PASS** — os alvos `pc2ps3_moving_platform_test`,
`pc2ps3_tilemap_test` e `pc2ps3_particles_test` compilaram em Debug; CTest
`moving_platforms`, `tilemap` e `particles` passou em 3/3. A execução de
`moving_platforms` cobre validação/capacidade, transporte horizontal e
vertical, perda de apoio, reset e determinismo.
Documentação: plataformas cinemáticas não implementam pathfinding nem colisão
contínua; a resolução segue discreta por tick.

## QA — tilemap e câmera

Escopo: `include/pc2ps3/tilemap.hpp`, `src/core/tilemap.cpp` e
`tests/tilemap_test.cpp`.

```gherkin
Funcionalidade: Desenhar tilemap com atlas e câmera

Cenário: Camadas são desenhadas por depth com empate estável
Dado que as camadas foram inseridas fora da ordem de depth
E duas camadas possuem o mesmo depth
Quando desenho o tilemap
Então camadas de menor depth aparecem primeiro
E empates preservam a ordem de inserção

Cenário: Scroll em pixels e clipping de tiles parciais
Dado que a câmera desloca o mapa por um pixel horizontal e vertical
E a viewport termina no meio de uma célula
Quando desenho o tilemap
Então os pixels usam os tiles corretos da atlas
E somente a parte dentro da viewport altera o framebuffer

Cenário: Células vazias e configuração inválida
Dado que -1 representa célula vazia e índices negativos menores que -1 são inválidos
Quando desenho uma célula vazia ou tento gravar coordenada inválida
Então nenhuma textura é desenhada e a coordenada inválida é rejeitada

Cenário: Limites de armazenamento e atlas
Dado um mapa acima dos limites de dimensões/células/camadas ou um tile fora da atlas
Quando crio a camada ou desenho o mapa
Então a operação falha sem alterar pixels da viewport
```

Execução da Tifa: **PASS** — alvo `pc2ps3_tilemap_test` compilado em CMake Debug;
CTest `tilemap` passou 1/1 com MSVC 19.44. Casos cobrem ordem de depth/empate,
scroll pixel, clipping, células vazias e rejeição de limites/atlas sem escrita.
Reexecutado no lote de 2026-11-02: CTest `tilemap` passou novamente (1/1).

## QA — sistema de partículas

Escopo: `include/pc2ps3/particles.hpp`, `src/core/particles.cpp` e
`tests/particles_test.cpp`.

```gherkin
Funcionalidade: Simular e desenhar partículas 2D

Cenário: Spawn limitado e reutilização do primeiro slot livre
Dado um sistema com capacidade dois
Quando adiciono duas partículas e tento uma terceira
Então as duas primeiras são aceitas e a terceira falha
Quando uma partícula expira e adiciono outra
Então o slot livre é reutilizado

Cenário: Movimento e lifetime determinísticos
Dado uma partícula com velocidade de 100 pixels por segundo e lifetime de 100 ms
Quando atualizo em dt=0 e depois em dt=10 ms
Então dt=0 não move e dt=10 move exatamente um pixel
Quando atualizo até o limite de 100 ms
Então a partícula expira nesse limite

Cenário: Entrada inválida e tempo fora do intervalo
Dado lifetime zero, tamanho inválido, velocidade acima do limite ou dt acima de 1000 ms
Quando tento spawn ou update
Então a operação falha sem corromper slots ativos

Cenário: Câmera, clipping e alpha
Dado uma partícula parcialmente fora da viewport e outra semitransparente
Quando desenho com offset de câmera
Então só os pixels visíveis mudam e o framebuffer compõe o alpha
```

Execução da Tifa: **PASS** — alvo `pc2ps3_particles_test` compilado em CMake Debug;
CTest `particles` passou 1/1 com MSVC 19.44. Casos cobrem slots, Q16.16,
expiração, dt inválido, câmera, clipping e alpha.
Reexecutado no lote de 2026-11-02: CTest `particles` passou novamente (1/1).

## QA — carregamento e pool de texturas

Escopo: `include/pc2ps3/texture_manager.hpp`,
`src/core/texture_manager.cpp`, integração com `Texture`/`FileSystem`,
`tests/texture_manager_test.cpp` e o alvo `texture_manager` no CTest.

```gherkin
Funcionalidade: Carregar, compartilhar e descarregar texturas PNG

Cenário: Carregamento preguiçoso e cache por path
  Dado um PNG válido em um FileSystem em memória
  Quando carrego o path pela primeira vez e depois peço o mesmo path de novo
  Então ocorre uma leitura, os handles apontam para a mesma textura e o cache
  mantém a entrada até que as duas referências sejam descarregadas

Cenário: Path inválido e falha de leitura preservam estado
  Dado uma textura carregada e um handle de saída vazio
  Quando peço path absoluto, traversal, segmento vazio, backslash ou arquivo ausente
  Então a operação falha, mantém a textura ativa e não inicia I/O para paths inválidos

Cenário: Handle antigo não acessa slot reutilizado
  Dado um handle cuja última referência foi descarregada
  Quando outro path reutiliza o slot
  Então o handle antigo retorna nulo e a nova geração dá acesso somente à nova textura

Cenário: Handle antigo não passa para uma nova instância no mesmo endereço
  Dado um manager que carregou uma textura e depois foi destruído
  Quando outro manager é construído no mesmo espaço de memória e carrega outra textura
  Então o handle antigo retorna nulo e só o handle novo acessa o recurso

Cenário: Buffer reutilizado e trim explícito
  Dado uma textura carregada e depois descarregada
  Quando carrego outra imagem do mesmo tamanho e depois chamo trim sem entradas ativas
  Então o pool preserva a capacidade durante a recarga e trim libera pixels e scratch

Cenário: Limites e erros não publicam handle parcial
  Dado um PNG acima do limite, uma imagem inválida, falha de I/O, cache cheio ou orçamento de pixels insuficiente
  Quando tento carregar a imagem
  Então recebo status distinto, a saída permanece inalterada e nenhuma textura parcial fica ativa
```

Execução da Tifa: **PASS** — MSVC 14.44 compilou o alvo manualmente com
`/std:c++14`; `build/qa/texture_manager.exe` retornou saída
`texture manager tests passed` e exit code 0, incluindo a regressão de identidade
entre instâncias. A primeira execução revelou que o
fixture de arquivo ausente tinha o único slot ocupado; o fixture foi corrigido
para dois slots e a suíte foi recompilada/executada novamente com PASS.

O alvo CTest permanente está em `CMakeLists.txt`. CMake/CTest não foi executado
porque CMake não está instalado no host; o modo `/std:c++11` também não existe
no MSVC usado, então a compilação manual foi em `/std:c++14`.

## QA — janela de salto

Escopo: `include/pc2ps3/jump_assist.hpp`, `src/core/jump_assist.cpp`,
integração em `World` e `tests/jump_assist_test.cpp`.

```gherkin
Funcionalidade: Jump buffer e coyote time determinísticos

Cenário: Pressão antes do pouso é consumida ao tocar o chão
  Dado um input de salto durante a descida dentro da janela de seis ticks
  Quando o jogador pousa sem manter o botão pressionado
  Então o salto ocorre no próximo tick e a entrada é consumida uma única vez

Cenário: Coyote time aceita até o último tick da janela
  Dado um jogador que acabou de perder o estado grounded
  Quando pressiona salto no sexto tick
  Então o salto é aceito
  Quando pressiona no tick seguinte após a janela expirar
  Então não há salto imediato

Cenário: Buffer expirado e reset
  Dado um input buffered durante o ar por mais de seis ticks
  Quando o jogador toca o chão, ou o sistema é resetado
  Então a entrada antiga não dispara um salto
```

Execução da Tifa: **PASS** — MSVC 14.44 (`/std:c++14`) compilou o núcleo e
`tests/jump_assist_test.cpp`; `build/qa/jump_assist.exe` retornou
`jump assist tests passed`, exit code 0. A compilação integral também emitiu
um warning C4244 preexistente em `src/core/audio.cpp:299`. CMake/CTest não foi
executado porque CMake não está instalado no host; MSVC não oferece
`/std:c++11`.

## QA — sensores e triggers

Escopo: `include/pc2ps3/trigger.hpp`, `src/core/trigger.cpp`,
`tests/trigger_test.cpp` e `CMakeLists.txt`.

```gherkin
Funcionalidade: Eventos determinísticos de sensores AABB

Cenário: Entrar, permanecer e sair dos sensores
  Dado dois sensores AABB registrados em ordem
  Quando o ator entra nos dois, permanece e depois sai
  Então enter e exit ocorrem uma vez por sensor e os estados estáveis não repetem eventos

Cenário: Atualização inválida preserva o estado
  Dado um ator dentro de sensores com eventos recém-gerados
  Quando update recebe uma AABB inválida
  Então retorna erro sem apagar eventos nem alterar sobreposições

Cenário: IDs e capacidade são limitados
  Dado um sistema com capacidade dois
  Quando cadastro IDs duplicados, limites inválidos ou um terceiro sensor
  Então cada operação inválida falha sem alterar os sensores existentes

Cenário: Reset limpa as sobreposições
  Dado o ator dentro dos sensores
  Quando o sistema é resetado e recebe a mesma AABB válida
  Então os sensores emitem enter novamente
```

Execução da Tifa: **PASS** — MSVC 14.44 (`/std:c++14`) compilou o núcleo
completo e `tests/trigger_test.cpp`; `build/qa/trigger_all.exe` retornou
`trigger tests passed`, exit code 0. O teste também confirmou que a capacidade
do vetor de eventos não cresce em `update()`. A compilação integral emitiu o
warning C4244 preexistente em `src/core/audio.cpp:299`. CMake/CTest não foi
executado porque CMake não está instalado; MSVC não oferece `/std:c++11`.

## QA — Camera2D

```gherkin
Feature: Seguimento de câmera 2D

  Scenario: Seguir alvo no centro do mundo
    Given viewport e mundo válidos
    When a câmera segue um alvo dentro dos limites
    Then o alvo fica centralizado com scroll inteiro

  Scenario: Clampar nas bordas e centralizar mundo menor
    Given alvo junto às bordas do mundo
    When a câmera o segue
    Then o scroll fica dentro dos limites; mundo menor que o viewport fica centralizado

  Scenario: Entrada inválida preserva câmera
    Given estado de câmera válido
    When follow recebe dimensões ou coordenadas inválidas
    Then a chamada falha e o estado anterior permanece
```

Execução QA local: o alvo `pc2ps3_camera_test` compilou em CMake Debug e o CTest
`camera` passou (1/1) com MSVC 19.44. CMake solicita C++11, mas MSVC não oferece
modo estrito `/std:c++11`; esse limite de toolchain permanece.

## QA — EnemySystem

```gherkin
Feature: Pool bounded de inimigos

  Scenario: Criar, atualizar e derrotar inimigo
    Given slot disponível e dados finitos válidos
    When o jogo atualiza a AABB e aplica dano suficiente
    Then o inimigo é derrotado e o slot fica reutilizável

  Scenario: Rejeitar ID duplicado ou dados inválidos
    Given um inimigo ativo
    When spawn recebe ID repetido, AABB inválido ou health não positivo
    Then a operação falha sem alterar o inimigo ativo

  Scenario: Respeitar capacidade e reutilizar slot
    Given o pool cheio
    When tento criar outro inimigo e depois derroto um ativo
    Then o primeiro spawn falha e o próximo reutiliza o slot liberado
```

Execução QA local: o alvo `pc2ps3_enemy_test` compilou em CMake Debug e o CTest
`enemies` passou (1/1) com MSVC 19.44. CMake solicita C++11, mas MSVC não oferece
modo estrito `/std:c++11`; esse limite de toolchain permanece. O demo integrado
continua cobrindo a derrota do inimigo pela hitbox de ataque.

## QA — InputManager: ações e analógico

```gherkin
Feature: Controles lógicos de teclado e DualShock 3

  Scenario: Mapear os botões padrão do DualShock 3
    Given Cross, Square, Circle, Triangle, L1, R1, Start e Select amostrados
    When InputMapper produz o InputFrame
    Then expõe confirmar/pular, ataque, cancelar, habilidade, L1/R1, pausa e menu auxiliar

  Scenario: Emitir edges pressed e estados held
    Given uma ação liberada
    When o botão é pressionado, mantido e liberado
    Then pressed ocorre só no primeiro tick e held acompanha os ticks pressionados

  Scenario: Combinar D-pad, teclado e eixos analógicos
    Given eixos normalizados e direções digitais
    When o mapeador calcula movimento
    Then preserva magnitude analógica, prioriza direção digital e neutraliza direções digitais opostas

  Scenario: Neutralizar oposição independente da quantidade de fontes
    Given teclado e D-pad enviam duas fontes para uma direção e uma para a direção oposta
    When InputMapper calcula o eixo digital
    Then move_x é zero e left/right ficam ambos inativos

  Scenario: Usar o eixo analógico no World
    Given move_x em meia intensidade
    When World avança um tick
    Then a aceleração horizontal é proporcional e InputFrame legado left/right continua funcionando

  Scenario: Resetar as bordas
    Given ações mantidas em pressed/held
    When InputMapper é resetado
    Then a próxima amostra pressionada gera novas bordas pressed
```

**Execução Debug 2026-11-02: PASS.** `input_mapping` e `input_actions` passaram
com assertions habilitadas. A regressão agora cobre oposição com duas fontes
digitais para um lado e uma para o outro; qualquer oposição no eixo resulta em
neutro, independentemente da contagem. A suíte host completa passou em 31/31
testes. Saída: `build/qa/full-suite/Testing/Temporary/LastTest.log`.

O build usa CMake 3.31.6, Visual Studio Build Tools 2022 e MSVC 19.44.35228.0;
`CMakeLists.txt` solicita C++11, mas MSVC não oferece modo estrito C++11. Não
contar testes Release como evidência: `NDEBUG` desativa `assert`. A dead-zone
física e leitura de botões por SDL2/PSL1GHT ainda dependem dos backends nativos.
## QA — scheduler, distribuição por tick e runtime loop (2026-11-02)
### FixedStepScheduler
Feature: converter elapsed em nanossegundos em updates fixos limitados

Scenario: nenhum tick antes do limite
Given fixed_step_ns=10, max_frame_ns=30 e max_steps_per_frame=2
When advance(9) é chamado
Then steps=0, alpha=0.9 e dropped_ns=0

Scenario: catch-up limitado e fração preservada
Given fixed_step_ns=10, max_frame_ns=30 e max_steps_per_frame=2
When advance(25) é chamado
Then steps=2, dropped_ns=0 e alpha=0.5

Scenario: ticks excedentes do catch-up são descartados
Given fixed_step_ns=10, max_frame_ns=30 e max_steps_per_frame=2
When advance(35) é chamado
Then steps=2, dropped_ns=15 e alpha=0

Scenario: frame acima do máximo e overflow
Given max_frame_ns=30 e elapsed_ns pode ser UINT64_MAX
When o frame excede o limite ou a soma interna aproximaria overflow
Then tempo acima do limite e ticks integrais excedentes são descartados sem overflow

Scenario: reset e configuração inválida
Given configuração válida, acumulador não zero, ou fixed_step_ns=0
When reset ou advance é chamado
Then reset reproduz a mesma sequência e configuração inválida não altera o estado

### InputTickDistributor
Feature: entregar bordas pressed uma vez por tick

Scenario: toque curto antes do próximo tick
Given held=false e pressed=true seguidos de uma amostra liberada antes do tick
When consume é chamado
Then o próximo tick recebe pressed=true e held=false, e o seguinte recebe pressed=false

Scenario: catch-up preserva estado contínuo e consome a borda uma vez
Given um input held com eixos e pressed=true submetido antes de vários ticks
When cada tick consome o distribuidor
Then todos recebem held/eixos atuais e somente o primeiro recebe pressed

Scenario: consumo vazio e reset
Given nenhuma amostra submetida ou estado previamente acumulado
When consume ou reset é chamado
Then consume vazio não altera a saída e reset remove estado/bordas

### RuntimeLoop
Feature: executar o ciclo de aplicação com host abstrato e cleanup previsível

Scenario: ciclo normal com catch-up
Given host falso fornece um frame com elapsed de dois ticks e depois quit
When RuntimeLoop::run executa
Then a ordem é initialize, start, poll, dois updates, render, present, poll, stop, shutdown

Scenario: render e apresentação sem tick
Given um frame válido com elapsed menor que fixed_step_ns
When o loop processa o frame
Then update não roda, mas render e present rodam uma vez

Scenario: erros e encerramento parcial
Given falha de initialize, start, poll, update, render ou present
When RuntimeLoop::run retorna
Then status identifica a falha e stop/shutdown rodam uma única vez somente para recursos iniciados

Scenario: configuração inválida antes do host
Given dimensão ou scheduler inválidos
When RuntimeLoop::run é chamado
Then retorna kInvalidConfiguration sem initialize, start ou shutdown

#### Execução local da QA (Tifa) — 2026-11-02
- CTest Debug focal: `fixed_step_scheduler`, `input_tick_distribution` e `runtime_loop` passaram (3/3).
- Suíte host CTest Debug completa passou em 34/34 após recompilar todos os alvos.
- Log: `build/qa/full-suite/Testing/Temporary/LastTest.log`.
- Ferramentas: CMake 3.31.6, Visual Studio Build Tools 2022, MSVC 19.44.35228.0.
- O projeto declara C++11, mas MSVC não oferece modo estrito `/std:c++11`; a suíte comprova build e execução host, não conformidade estrita de outro compilador.
- SDL2/PSL1GHT, polling de dispositivo, pacing, RSX e toolchain/hardware PS3 não foram exercitados.
## QA — alpha de render, World e demo RuntimeLoop (2026-11-02)
### Render alpha
Feature: expor ao render a fração restante do timestep fixo
Scenario: render recebe alpha após dois ticks
Given fixed_step_ns=10, max_frame_ns=30 e max_steps_per_frame=2
When um frame válido tem elapsed_ns=25
Then dois updates ocorrem, render recebe alpha=0.5 e present ocorre uma vez

Scenario: render sem update
Given o mesmo scheduler
When um frame válido tem elapsed_ns=5
Then nenhum update ocorre, render recebe alpha=0.5 e present ocorre uma vez

### World interpolation
Feature: consultar posição interpolada sem alterar física
Scenario: interpolar entre dois updates
Given posições anterior e atual do player registradas pelo World
When interpolated_player_position recebe alpha=0.5
Then a saída é o ponto médio e posição física, velocidade, grounded e tick permanecem iguais

Scenario: rejeitar alpha inválido e resetar histórico
Given alpha negativo, alpha >=1 ou NaN, ou World após reset
When a consulta é chamada
Then alpha inválido falha sem alterar a saída e reset alinha posição anterior/atual

### Demo headless integrado
Feature: demonstrar RuntimeLoop com host scripted
Scenario: executar input, catch-up, frame sem tick e quit
Given snapshots e elapsed fixos fornecidos pelo host de teste
When pc2ps3_runtime_demo é executado
Then World avança só em fixed updates, render usa alpha, o loop termina em quit e gera PPM/JSON determinísticos
#### Execução local da QA (Tifa) — interpolação, 2026-11-02
- CTest Debug focado: runtime_loop, world_interpolation e runtime_demo_smoke passaram (3/3).
- Suíte host CTest Debug recompilada completa: 36/36 passaram. Log: build/qa/full-suite/Testing/Temporary/LastTest.log.
- O demo imprimiu {"status":"quit","frames":3,"ticks":2,"jump_pressed_updates":1,"presents":3,"last_alpha":0.500,"player_x":66.048,"render_x":65.408} e gerou build/qa/full-suite/pc2ps3_runtime_demo.ppm (172815 bytes); o render interpolado fica entre as posições física anterior e atual.
- Ferramentas: CMake 3.31.6, Visual Studio Build Tools 2022, MSVC 19.44.35228.0. MSVC não oferece modo estrito /std:c++11; hardware/backend PS3 não exercitados.

### HostFileSystem — cenários de Tifa

Feature: operar arquivos host sob uma raiz configurada sem expor caminhos externos.

Scenario: ler arquivo no limite permitido
Given um arquivo de três bytes dentro da raiz
When read_all recebe max_bytes igual a três
Then retorna kOk e substitui a saída pelo conteúdo completo

Scenario: recusar arquivo acima do limite
Given uma saída sentinela e um arquivo maior que max_bytes
When read_all tenta carregá-lo
Then retorna kTooLarge e mantém a saída sentinela intacta

Scenario: rejeitar traversal e caminhos absolutos
Given caminhos com .., prefixo de unidade, separador invertido ou componente vazio
When read_all, write_atomic ou remove os recebem
Then retorna kError sem alterar o arquivo anterior

Scenario: gravar por substituição e remover
Given um save antigo e o diretório pai já existente
When write_atomic grava um save novo e remove o destino
Then o arquivo contém os bytes novos, remove retorna kOk e a segunda remoção retorna kNotFound

Scenario: rejeitar links e pontos de reparse
Given o Windows permite criar hard link, symlink ou junction no diretório temporário
When a API tenta ler, gravar ou remover através desse caminho
Then retorna kError e preserva o arquivo externo

#### Execução local da QA (Tifa) — HostFileSystem, 2026-11-02

- Build Debug do alvo host_file_system passou sem warnings.
- CTest focado passou em 1/1; a suíte host CTest Debug passou em 37/37.
- Os caminhos inválidos, limites, saída inalterada, gravação inicial/substituição, remoção e ciclo de vida do diretório temporário foram exercitados.
- O Windows negou criação de symlink, hard link e junction neste ambiente; esses subcenários foram registrados como SKIP, portanto a rejeição de reparse points não foi demonstrada em execução.
- Evidência completa: build/qa/full-suite/Testing/Temporary/LastTest.log. Backend SDL2/PSL1GHT, PS3 e concorrência maliciosa sobre a raiz não foram testados.

### SaveManager via HostFileSystem — cenários de Tifa

Feature: persistir saves no host sem depender de FileSystem em memória.

Scenario: carregar save por nova instância
Given SaveManager grava um payload em diretório temporário usando HostFileSystem
When a primeira instância é destruída e outra instância carrega o mesmo slot
Then o payload carregado é idêntico ao payload salvo

Scenario: preservar saída ao ler save corrompido
Given o registro no disco não tem envelope de save válido e a saída contém uma sentinela
When uma nova instância chama load
Then retorna kCorruptData e a saída continua igual à sentinela

#### Execução local da QA (Tifa) — SaveManager, 2026-11-02

- CTest Debug focado: host_save_persistence passou em 1/1.
- Toda a gravação ocorreu dentro de pasta temporária sob o build; o teste limpa o save no encerramento.

### TextureManager via HostFileSystem — cenários de Tifa

Feature: carregar e reutilizar texturas PNG reais por caminhos lógicos.

Scenario: decodificar PNG em disco
Given um PNG original 1x1 gravado no diretório temporário através de HostFileSystem
When TextureManager carrega o path lógico
Then dimensão e pixel RGBA coincidem com a fixture

Scenario: compartilhar cache e recarregar após unload
Given duas leases carregam o mesmo path
When uma lease é liberada, depois a outra, e uma nova carga é solicitada
Then a textura compartilhada continua válida até a última liberação e a nova carga volta a decodificar o PNG

Scenario: rejeitar path com traversal
Given o chamador pede um caminho com componente ..
When TextureManager valida o path
Then retorna kInvalidArgument e deixa o handle inválido

#### Execução local da QA (Tifa) — TextureManager, 2026-11-02

- CTest Debug focado: host_texture_loading passou em 1/1.
- A fixture PNG está no teste; nenhuma imagem comercial foi lida ou copiada.

#### Validação final do lote integrado de três cards — 2026-11-02

- Build CMake Debug completo sem warnings.
- CTest da suíte completa passou em 39/39; log: build/qa/full-suite/Testing/Temporary/LastTest.log.
- Testes de symlink, hard link e junction reportaram SKIP porque o host negou sua criação. A rejeição desses caminhos permanece sem execução neste ambiente.


## Fechamento CTest — vertical slice, hitbox e knockback (2026-11-02)

### Tifa — cenarios e execucao

```gherkin
Feature: Demo vertical slice original
  Scenario: Simulacao deterministica chega a vitoria
    Given player, inimigo, ataque, checkpoint e camera configurados
    When o demo executa 60 ticks
    Then o inimigo e derrotado, o checkpoint 10 fica ativo e o estado chega a vitoria
    And o processo grava prototype.ppm e imprime o estado final em JSON

Feature: Hitbox e hurtbox
  Scenario: Emitir pares de colisao estaveis sem self-hit
    Given hitboxes e hurtboxes validas registradas no tick
    When update e executado
    Then pares entre owners diferentes sao ordenados e pares do mesmo owner sao ignorados

  Scenario: Rejeitar registro invalido e limpar eventos no tick seguinte
    Given capacidade limitada e eventos anteriores
    When registro excede capacidade ou update executa sem pares
    Then falha nao corrompe os demais registros e os eventos sao substituidos

Feature: Knockback
  Scenario: Aplicar impulso e integrar com World
    Given Actor com velocidade e grounded configurados
    When impulso finito horizontal/vertical e aplicado
    Then velocidades somam o impulso e componente vertical remove grounded

  Scenario: Rejeitar nao finito e overflow
    Given Actor com estado valido
    When entrada ou resultado excede o intervalo representavel
    Then a chamada falha sem alterar o Actor
```

- `ctest -R "prototype_vertical_slice|hitboxes|knockback"`: PASS, 3/3.
- `prototype_vertical_slice` imprimiu `tick=60`, `state=3`, `enemy_defeated=true`,
  `checkpoint_active=true`, `checkpoint_id=10`, camera `(28,180)` e
  `animation_frame=1`; `prototype.ppm` foi gerado com 172815 bytes no build dir.
- `hitboxes` e `knockback` passaram pelos executaveis C++ Debug registrados no CTest.
- MSVC 19.44 compila os alvos; o modo estrito `/std:c++11` nao esta disponivel.
```


## Fechamento CTest — StateMachine, checkpoints e projeteis (2026-11-02)

### Tifa — cenarios e execucao

```gherkin
Feature: StateMachine limitada
  Scenario: Executar apenas transicoes declaradas e resetar
    Given grafo finito com transicoes 0->1 e 1->2
    When transicao proibida ou estado invalido e solicitado
    Then o estado atual permanece; reset volta ao estado inicial

Feature: Checkpoints
  Scenario: Ativar checkpoint ao entrar no trigger
    Given pontos de respawn com IDs unicos e TriggerSystem configurado
    When ocorre evento enter para checkpoint conhecido
    Then respawn muda para aquele ponto e reset restaura spawn inicial

  Scenario: Rejeitar checkpoint invalido
    Given capacidade finita e checkpoint ativo
    When ID duplicado, posicao invalida, excesso de capacidade ou ID desconhecido e enviado
    Then os pontos e respawn atuais permanecem intactos

Feature: ProjectileSystem
  Scenario: Mover, colidir, consumir e expirar projeteis
    Given pool limitado e projeteis com dados validos
    When update avanca ticks e collide recebe AABB valido
    Then hits sao ordenados, projeteis atingidos sao consumidos e lifetime expira

  Scenario: Rejeitar dt/AABB invalido e desenhar com clipping/alpha
    Given projeteis ativos
    When dt ou AABB e invalido e depois o framebuffer recebe draw
    Then estado e preservado na falha e desenho respeita clipping/alpha
```

- `ctest -R "state_machine|checkpoints|projectiles"`: PASS, 3/3 em CMake Debug.
- StateMachine: configuracao valida/invalida, transicoes permitidas/proibidas e reset.
- Checkpoints: IDs, capacidade, posicoes, evento enter do TriggerSystem e fallback inicial.
- ProjectileSystem: movimento Q16.16, colisao/ordem/consumo, expiracao, dt/AABB invalidos, draw com clipping/alpha e clear.
- O contrato de projeteis continua com colisao discreta por tick; tunelamento e dano pertencem a cards futuros/jogo.
- MSVC 19.44 nao fornece modo estrito `/std:c++11`.
```


## QA — plataformas moveis em CMake/CTest (2026-11-02)

```gherkin
Feature: Plataformas moveis cinematicas
  Scenario: Atualizar movimento e restaurar posicoes
    Given plataformas validas ordenadas e World em timestep fixo
    When update avanca e depois reset e chamado
    Then plataformas movem deterministicamente e voltam a posicao inicial

  Scenario: Transportar jogador apoiado e perder apoio
    Given jogador grounded sobre uma plataforma
    When a plataforma move horizontal ou verticalmente e depois deixa de ser suporte
    Then jogador acompanha o deslocamento e volta ao comportamento normal sem suporte

  Scenario: Rejeitar entrada invalida e capacidade cheia
    Given lista com capacidade limitada
    When AABB/velocidade nao finitos ou capacidade excedida sao enviados
    Then a operacao falha sem alterar plataformas existentes
```

Execucao Tifa: alvo `pc2ps3_moving_platform_test` compilado em CMake Debug;
CTest `moving_platforms` passou 1/1. O teste cobre validacao, transporte, perda de
apoio, reset e determinismo. MSVC 19.44 nao fornece `/std:c++11` estrito;
plataformas seguem sem pathfinding ou colisao continua.

## Tifa - texto 3x5 do Framebuffer

```gherkin
Feature: Desenhar texto ASCII no framebuffer
  Scenario: Renderizar fonte, normalizar minusculas e usar fallback
    Given um framebuffer limpo
    When texto com letras, digitos, pontuacao, minusculas e byte nao suportado e desenhado
    Then minusculas coincidem com maiusculas e bytes sem glifo usam o ponto de interrogacao

  Scenario: Escalar, quebrar linhas e recortar o texto
    Given um texto com escala configurada e linhas multiplas
    When o texto e desenhado parcialmente fora dos limites
    Then os glifos mantem a escala, a segunda linha inicia abaixo da primeira e pixels externos nao sao escritos

  Scenario: Rejeitar entrada invalida sem alterar o framebuffer
    Given um framebuffer com cor sentinela
    When ponteiro nulo com tamanho positivo, texto acima de 1024 bytes ou escala fora de 1 a 8 e enviado
    Then a chamada falha e todos os pixels permanecem iguais
```

Execucao Tifa: `render_text` e `debug_hud` passaram em CMake Debug, 2/2.
O primeiro build compilou `pc2ps3_render_text_test`; depois o alvo de HUD foi
compilado e a regressao repetida. A suite cobre glifos e fallback, escala,
multilinha, clipping e entradas invalidas sem escrita. MSVC 19.44 nao oferece
modo estrito `/std:c++11`.

## Tifa - overlay RGBA de tela inteira

```gherkin
Feature: Compor overlay RGBA no framebuffer
  Scenario: Alpha transparente preserva a imagem
    Given um framebuffer com pixels sentinela
    When blend_overlay recebe alpha zero
    Then todos os pixels permanecem iguais

  Scenario: Alpha intermediario e opaco usam a composicao straight-over
    Given um framebuffer preenchido e uma cor de overlay
    When blend_overlay recebe alpha 128 e depois alpha 255
    Then o alpha intermediario coincide com blend_rect e o opaco substitui os pixels

  Scenario: Overlay em framebuffer vazio ou degenerado
    Given um framebuffer sem pixels por largura ou altura zero
    When blend_overlay e chamado
    Then a operacao termina sem acesso invalido
```

Execucao Tifa: CMake Debug compilou `pc2ps3_vfx_overlay_test`; CTest
`vfx_overlay` passou em 1/1. Alpha zero preservou os pixels, alpha 128
coincidiu com `blend_rect()`, alpha 255 substituiu toda a tela, e framebuffers
com largura ou altura zero terminaram sem acesso invalido.

## Tifa - FrameTimeTracker

```gherkin
Feature: Medir duracao de frames sem fornecer relogio ao core
  Scenario: Estatisticas de uma janela parcial
    Given um tracker vazio e duracoes validas em nanossegundos
    When amostras sao adicionadas
    Then contagem, ultima, media inteira, minimo e maximo refletem a janela

  Scenario: Substituir a amostra mais antiga ao completar 120
    Given 120 amostras validas na janela circular
    When uma 121a amostra e adicionada
    Then a contagem fica em 120 e as metricas excluem a amostra mais antiga

  Scenario: Rejeitar duracao invalida sem alterar estado e resetar
    Given um tracker com amostras acumuladas
    When duracao zero ou acima de 10 segundos e enviada e depois reset e chamado
    Then a rejeicao preserva as metricas e reset limpa todos os valores
```

Execucao Tifa: CMake Debug compilou `pc2ps3_frame_time_test`; CTest
`frame_time` passou em 1/1. O teste cobriu janela parcial, limite de 120 e
substituicao da amostra mais antiga, media inteira, minimo/maximo, rejeicao de
zero e acima de 10 segundos sem mutacao e reset.

## Execucao consolidada   texto, overlay e frame time (2026-11-02)

CMake Debug/CTest executou `debug_hud`, `render_text`, `vfx_overlay` e
`frame_time`: PASS, 4/4. Build com CMake 3.31.6 e MSVC 19.44.35228.0.
O modo estrito `/std:c++11` e SDL2/PSL1GHT/RSX/PS3 nao foram exercitados.
