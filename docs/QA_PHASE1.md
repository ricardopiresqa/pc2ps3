# QA — contratos portáveis da Fase 1

## Cenários Gherkin

```gherkin
Feature: Adaptadores portáveis entre o core e superfícies de dispositivo

  Scenario: Mapear controle canônico sem sobrescrever teclado
    Given um InputSnapshot com estado de teclado ativo
    And uma amostra canônica com D-pad, eixos e botões do controle
    When aplico a amostra ao snapshot
    Then os campos DualShock 3 refletem a amostra
    And os campos de teclado permanecem iguais
    And a ponte não aplica dead-zone nem acessa API de dispositivo

  Scenario: Renderizar áudio em blocos limitados
    Given um AudioManager com uma fonte PCM determinística
    When o callback pede mais frames que max_chunk_frames
    Then cada chamada ao AudioManager recebe no máximo max_chunk_frames
    And os frames produzidos permanecem na ordem original

  Scenario: Silenciar buffer quando a renderização falha
    Given uma entrada inválida ou uma fonte que lança exceção
    When executo o callback de áudio
    Then o retorno é falso
    And todo o buffer válido contém silêncio

  Scenario: Silenciar quando o AudioManager rejeita o render
    Given um AudioManager inválido e um buffer com dados residuais
    When o callback solicita frames
    Then o retorno é falso
    And todo o buffer válido contém silêncio

  Scenario: Rejeitar overflow ao calcular amostras PCM
    Given uma quantidade de frames maior que metade do limite de size_t
    When o callback calcula a quantidade de amostras estéreo
    Then o retorno é falso
    And o buffer não é acessado

  Scenario: Copiar RGBA/BGRA respeitando stride
    Given um framebuffer 2x2 e uma superfície caller-owned com padding
    When copio nas ordens RGBA8888 e BGRA8888
    Then os canais e linhas correspondem aos pixels do framebuffer
    And o padding permanece inalterado

  Scenario: Rejeitar superfície curta antes de escrever
    Given destino, stride ou capacidade insuficientes
    When solicito a apresentação do framebuffer
    Then o retorno é falso
    And todos os bytes do destino permanecem inalterados

  Scenario: Rejeitar overflow no stride da superfície
    Given um framebuffer válido e um stride próximo do limite de size_t
    When calculo os bytes necessários para todas as linhas
    Then o retorno é falso
    And todos os bytes do destino permanecem inalterados
```

## Execução

- Testes permanentes: `tests/platform_input_adapter_test.cpp`,
  `tests/audio_callback_test.cpp` e `tests/framebuffer_presenter_test.cpp`.
- Alvos registrados em CTest: `platform_input_adapter`, `audio_callback` e
  `framebuffer_presenter`.
- Ambiente validado em 2026-11-02: CMake 3.31.6, Visual Studio Build Tools
  2022 (MSVC 19.44.35228.0), Windows SDK 10.0.26100.0.
- Build Debug e CTest da suíte host inteira: 31/31 PASS, incluindo
  `platform_input_adapter`, `audio_callback` e `framebuffer_presenter`.
- Saída detalhada: `build/qa/full-suite/Testing/Temporary/LastTest.log`.
- Rebuild focal Debug nesta sessão: os três alvos foram compilados e o CTest
  passou 3/3 (`platform_input_adapter`, `audio_callback` e
  `framebuffer_presenter`). O log atual está em
  `build/qa/phase1-adapters/Testing/Temporary/LastTest.log`.
- Os cenários de áudio agora exercitam `AudioManager::render() == false` e o
  overflow de `frame_count * 2`; o presenter também rejeita overflow no stride
  sem alterar o destino.
- Comandos usados:
  - `cmake -S . -B build/qa/full-suite -G "Visual Studio 17 2022" -A x64 -DPC2PS3_BUILD_TESTS=ON -DPC2PS3_BUILD_TOOLS=ON`
  - `cmake --build build/qa/full-suite --config Debug`
  - `ctest --test-dir build/qa/full-suite -C Debug --no-tests=error --output-on-failure`
- `cmake --build build/qa/phase1-adapters --config Debug --target pc2ps3_platform_input_test pc2ps3_audio_callback_test pc2ps3_framebuffer_presenter_test`
- `ctest --test-dir build/qa/phase1-adapters -C Debug -R '^(platform_input_adapter|audio_callback|framebuffer_presenter)$' --output-on-failure`
- A suíte usa `assert`; Release define `NDEBUG`, então resultados Release não
  foram usados como evidência de QA.
- Nenhum cenário de hardware, SDL2, PSL1GHT, RSX ou PKG está coberto por estes
  contratos.

## Backend de host Windows

```gherkin
Feature: exercitar RuntimeLoop em janela Windows sem depender de SDL2
  Scenario: apresentar framebuffer em uma janela oculta
    Given um framebuffer pequeno e válido
    When o adapter inicializa, apresenta o quadro e encerra
    Then as operações têm sucesso e a janela é destruída

  Scenario: preservar canais e orientação top-down
    Given pixels de cores diferentes em cada posição do framebuffer
    When os pixels são convertidos para BGRA
    Then canais e ordem das linhas são preservados

  Scenario: rejeitar dimensões inválidas atomicamente
    Given largura zero ou armazenamento incompatível
    When a plataforma tenta inicializar ou converter
    Then retorna falha sem modificar a saída anterior

  Scenario: processar fechamento da janela
    Given uma janela oculta inicializada
    When WM_CLOSE é processado
    Then poll retorna kQuit e shutdown libera a janela

  Scenario: mapear teclado sem alterar campos do controle
    Given estados sintéticos de esquerda, salto, ataque e pausa
    When o mapeador preenche InputSnapshot
    Then as ações do teclado são corretas e o estado DualShock 3 permanece intacto
```

O teste automatizado do card é `pc2ps3_win32_framebuffer_test`; executa com
janela oculta e não requer interação visual. A demo interativa é
`pc2ps3_pc_demo` e usa física genérica, sem valores medidos de jogos comerciais.

## Revalidação do adapter Win32 — 2026-11-03

- Toolchain host: Visual Studio 2022 Build Tools, MSVC 19.44.35228, Windows
  SDK 10.0.26100; CMake 3.31.6 fornecido pelo Visual Studio.
- Build Debug dos alvos `pc2ps3_pc_demo`,
  `pc2ps3_win32_framebuffer_test`, `pc2ps3_runtime_demo`,
  `pc2ps3_core_test` e `pc2ps3_runtime_loop_test`: PASS.
- CTest focado `win32_framebuffer`: 1/1 PASS. Cobriu conversão RGBA→BGRA,
  ordem top-down, buffer reutilizado, dimensão incompatível, teclado,
  relógio, janela oculta, apresentação, WM_CLOSE, shutdown e reinitialize.
- CTest de compatibilidade headless `core_determinism` e `runtime_loop`: 2/2
  PASS.
- A regressão host completa não foi executada neste build; somente esses
  alvos foram compilados para esta entrega.
- `ppu-gcc`, `ppu-g++`, ferramentas SELF/PKG e RPCS3 não estão no PATH; o Docker
  instalado não encontra o daemon Linux. Backend PS3 e `.pkg` continuam sem
  build verificável. MSVC também não prova o gate C++11 estrito.

## Win32 XInput

```gherkin
Feature: Adaptar XInput para o contrato canônico sem exigir controle físico
  Scenario: Mapear botões e direções de um controle conectado
    Given um estado XInput sintético com D-pad, stick, A/B/X/Y, LB/RB, gatilhos, Start e Back
    When o adapter produz um PlatformControllerSample
    Then cada botão lógico segue o mapeamento documentado, incluindo gatilhos a partir do limiar 30
    And o eixo vertical fica positivo para baixo
  Scenario: Aplicar dead-zone e preservar limites dos eixos
    Given um stick dentro da dead-zone ou nos limites -32768 e 32767
    When o estado é normalizado
    Then valores internos à dead-zone viram zero e os extremos são preservados sem overflow
  Scenario: Neutralizar sample em desconexão e erro
    Given uma leitura que retorna desconectado ou erro
    When o adapter é consultado
    Then o status identifica a condição e o sample fica neutro
  Scenario: Reconectar e preservar teclado
    Given uma leitura conectada após desconexão e um InputSnapshot com teclado ativo
    When o sample é aplicado
    Then o controle volta a aparecer e os campos de teclado permanecem iguais
```

Execução — 2026-11-03: MSVC Debug compilou `pc2ps3_win32_gamepad_test` e `pc2ps3_pc_demo`. CTest focal passou 4/4 (`win32_gamepad`, `win32_framebuffer`, `core_determinism`, `runtime_loop`). O teste usa leitor XInput sintético; não precisou de controle conectado ou janela visível.

## Pacing de frames Win32

```gherkin
Feature: limitar o ritmo de frames sem bloquear eventos da janela
  Scenario: calcular espera com arredondamento para cima
    Given um relógio QPC sintético, período de 60 FPS e deadline futuro
    When o scheduler converte os ticks restantes para timeout
    Then o timeout é arredondado para cima e nunca vira busy-spin
  Scenario: desligar pacing e rejeitar configuração inválida
    Given FPS zero ou acima do limite permitido
    When inicializo o runtime
    Then zero não espera e valor inválido falha antes da criação da janela
  Scenario: não recuperar frames atrasados em rajada
    Given o relógio passou do deadline por mais de um período
    When calculo o próximo timeout
    Then o novo deadline parte do relógio atual e não solicita ticks acumulados
  Scenario: continuar respondendo ao fechamento durante a espera
    Given uma janela oculta com pacing habilitado
    When WM_CLOSE é postado
    Then poll retorna kQuit e shutdown libera a janela
```

Execução — 2026-11-03: MSVC Debug compilou `pc2ps3_win32_frame_pacer_test` e `pc2ps3_pc_demo`. CTest focal passou 5/5 (`win32_frame_pacer`, `win32_gamepad`, `win32_framebuffer`, `core_determinism`, `runtime_loop`). Testes sintéticos validaram deadlines, late-frame reset, pacing desligado, overflow, FPS inválido e fechamento em janela oculta.

## Saída WinMM PCM

```gherkin
Feature: enviar PCM16 estéreo em blocos pré-alocados sem exigir dispositivo real
  Scenario: abrir, preencher, reciclar e fechar a fila
    Given um AudioManager válido e uma fake WaveOut API
    When inicializo a saída, marco um bloco como concluído e chamo pump
    Then apenas esse bloco é renderizado e submetido novamente
    And shutdown desarma, desprepara e fecha cada recurso uma vez
  Scenario: rejeitar configuração inválida antes de abrir WinMM
    Given taxa, tamanho de bloco ou profundidade fora dos limites
    When inicializo a saída
    Then retorna erro sem chamar open
  Scenario: limpar falha parcial de preparação ou escrita
    Given a fake falha durante prepare ou write
    When initialize falha
    Then reset, unprepare e close são tentados e não há bloco ativo
  Scenario: recuperar erro transitório no shutdown
    Given reset, unprepare ou close relata erro transitório
    When shutdown repete a operação com segurança
    Then todos os headers são liberados e o shutdown termina
  Scenario: manter headers vivos se o driver continuar ocupado
    Given unprepare falha em todas as tentativas limitadas
    When shutdown termina sem conseguir desarmar o header
    Then retorna false sem liberar a memória ainda referenciada
    And uma chamada posterior pode completar a limpeza
  Scenario: preservar o estado após recusa persistente de close
    Given close falha em todas as tentativas limitadas após unprepare
    When shutdown retorna false
    Then os recursos restantes continuam sob ownership para retry
    And uma chamada posterior fecha o dispositivo e termina a limpeza
```

## Opções do demo Win32

```gherkin
Feature: configurar o pacing do demo antes de abrir a janela
  Scenario: usar o limite padrão
    Given nenhum argumento de FPS
    When o demo analisa as opções
    Then usa 60 FPS
  Scenario: aceitar pacing desativado e limites suportados
    Given --max-fps recebe 0, 1 ou 240
    When o demo analisa as opções
    Then repassa o valor validado ao Win32RuntimePlatform
  Scenario: rejeitar entrada inválida sem mutar opções
    Given o valor está ausente, malformado, duplicado ou fora de 0..240
    When o demo analisa as opções
    Then falha antes de criar a janela e mantém as opções originais
```

## XInput no runtime Win32

```gherkin
Feature: mesclar um controle opcional ao snapshot do runtime Windows
  Scenario: usar teclado sem DLL ou controle conectado
    Given o leitor XInput está indisponível
    When a plataforma consulta um frame
    Then entrega snapshot neutro de controle e mantém o runtime ativo
  Scenario: combinar teclado e controle
    Given o leitor retorna botões e eixo do controle 0
    When a plataforma consulta um frame
    Then os campos DualShock do InputSnapshot refletem o controle
    And os campos de teclado permanecem independentes
  Scenario: desconectar e reconectar
    Given o controle estava conectado e depois desconecta
    When a plataforma consulta frames sucessivos
    Then a desconexão produz sample neutro
    And a reconexão volta a produzir estado observável
```

Execução — 2026-11-03: MSVC Debug compilou `pc2ps3_win32_runtime_platform_test`, `pc2ps3_win32_gamepad_test`, `pc2ps3_platform_input_test`, `pc2ps3_runtime_loop_test` e o demo. CTest focal passou 4/4 (`win32_runtime_platform`, `win32_gamepad`, `platform_input_adapter`, `runtime_loop`). A integração usa o primeiro controle e consulta input quando a janela está em foreground; fake XInput cobre conexão, desconexão e reconexão, sem controle físico.

## Áudio no demo Win32

```gherkin
Feature: manter o demo ativo quando a saída WinMM não está disponível
  Scenario: iniciar sem dispositivo de áudio
    Given waveOut open falha
    When DemoApplication.start inicializa o AudioManager
    Then o demo continua sem saída de áudio
    And runtime start/update/render continuam válidos
  Scenario: renderizar e encerrar com saída ativa
    Given WinMM aceita a fila PCM16
    When render e stop são chamados no thread do loop
    Then pump recicla a fila e shutdown termina antes do AudioManager
  Scenario: desativar saída após erro no pump
    Given pump retorna erro
    When o demo renderiza o frame
    Then tenta shutdown e continua apresentando o framebuffer
```

Execução — 2026-11-03: MSVC Debug compilou `pc2ps3_pc_demo`, `pc2ps3_win32_audio_output_test` e `pc2ps3_runtime_demo_options_test`. CTest focal passou 6/6 (`win32_audio_output`, `audio_callback`, `audio_mixing`, `runtime_loop`, `runtime_demo_options`, `win32_runtime_platform`). A fake WinMM exercitou ausência/falhas e cleanup sem hardware. O demo foi compilado, mas não aberto em janela nem testado com dispositivo físico; seu mixer não gera clips e, portanto, a fila fica silenciosa.

Execução — 2026-11-03: MSVC Debug compilou `pc2ps3_win32_audio_output_test` e `pc2ps3_pc_demo`. CTest focal passou 8/8 (`win32_audio_output`, `audio_callback`, `audio_mixing`, `win32_frame_pacer`, `win32_gamepad`, `win32_framebuffer`, `core_determinism`, `runtime_loop`). A fake WinMM cobriu open, prepare, write, reset, unprepare, close, reciclagem, recusa persistente de unprepare/close e retry sem dispositivo físico. A suíte CTest inteira não foi validada: há testes para executáveis que não foram construídos neste lote.

## Opções de atlas no demo Win32

```gherkin
Feature: configurar assets locais antes de abrir a janela
  Scenario: manter o demo padrão sem consultar o filesystem
    Given nenhum argumento de atlas
    When o demo analisa as opções
    Then mantém o pacing padrão de 60 FPS e o índice de sprite 0
    And não cria uma configuração de assets
  Scenario: aceitar raiz, índice lógico e sprite dentro da capacidade
    Given --asset-root e --atlas-index estão presentes
    And --sprite-index está entre 0 e 1023
    When o demo analisa as opções
    Then publica todos os valores validados sem abrir arquivos
  Scenario: exigir raiz e índice em conjunto
    Given somente --asset-root ou --atlas-index foi informado
    When o demo analisa as opções
    Then rejeita os argumentos sem alterar as opções recebidas
  Scenario: rejeitar ordinal fora do limite
    Given --sprite-index está vazio, malformado ou acima de 1023
    When o demo analisa as opções
    Then rejeita os argumentos sem alterar as opções recebidas
  Scenario: rejeitar argumentos ausentes, duplicados ou desconhecidos
    Given uma opção está repetida, sem valor, ou não é suportada
    When o demo analisa as opções
    Then falha antes da criação da janela e framebuffer
```

## Carregamento do atlas no demo Win32

```gherkin
Feature: carregar um atlas local sem perder o fallback vetorial
  Scenario: carregar índice e PNG válidos
    Given um índice PC2SATL válido e sua PNG irmã compatível
    When o demo inicia com uma raiz confinada e sprite existente
    Then publica textura e região juntos
    And lê os arquivos somente durante o start
  Scenario: manter fallback quando o índice não existe ou é inválido
    Given o índice está ausente ou corrompido
    When o carregador tenta abrir o atlas
    Then não publica textura nem metadata parcial
    And o demo mantém o jogador vetorial
  Scenario: manter fallback quando a PNG irmã está ausente
    Given o índice é válido mas sua imagem irmã não existe
    When o carregador resolve o atlas
    Then descarrega qualquer recurso temporário
    And o demo mantém o jogador vetorial
  Scenario: rejeitar ordinal sem região correspondente
    Given o índice contém menos sprites que o ordinal selecionado
    When o carregador valida o sprite
    Then descarrega o handle temporário
    And deixa textura e metadata indisponíveis
  Scenario: descarregar antes da destruição dos donos
    Given o atlas foi carregado
    When o demo encerra
    Then descarrega o handle antes do TextureManager e do FileSystem
```

## Renderização do sprite no demo Win32

```gherkin
Feature: desenhar o sprite ordinal no lugar do jogador vetorial
  Scenario: renderizar a região selecionada
    Given o índice e a PNG estão válidos e o ordinal existe
    When o demo renderiza o jogador
    Then escala a região com draw_texture_region_scaled no retângulo do jogador
    And não relê arquivos durante os frames
  Scenario: recortar uma região válida na borda do framebuffer
    Given a região toca a borda da textura e a posição toca a borda do framebuffer
    When o demo renderiza o sprite
    Then somente os pixels visíveis são escritos dentro do framebuffer
  Scenario: usar fallback sem atlas ou com seleção inválida
    Given o atlas está ausente ou não contém o ordinal selecionado
    When o demo renderiza o jogador
    Then desenha o jogador vetorial atual
  Scenario: rejeitar uma região de textura fora dos limites
    Given as coordenadas de origem não cabem na textura
    When draw_texture_region_scaled valida a região
    Then retorna falha sem escrever pixels parciais
```

Execução — 2026-11-03: MSVC Debug compilou `pc2ps3_pc_demo`, `pc2ps3_runtime_demo_options_test`, `pc2ps3_runtime_demo_atlas_test`, `pc2ps3_texture_manager_test`, `pc2ps3_host_file_system_test`, `pc2ps3_atlas_runtime_index_test` e `pc2ps3_render_texture_test`. CTest focado passou 6/6 (`runtime_demo_options`, `runtime_demo_atlas`, `texture_manager`, `host_file_system`, `atlas_runtime_index`, `render_texture`). A validação usou índices/PNG sintéticos e pixels no framebuffer; não abriu janela nem usou assets comerciais do build Unity analisado.
