# QA — Fase 3: pacote de assets PC2PAK

## Sprite region reader v21 — 2026-11-03

### Cenários Tifa
- Região inteira de Sprite local em little/big-endian retorna somente x, y, largura e altura e para antes do sentinel do objeto seguinte.
- Coordenada negativa, fracionária, NaN/Inf, retângulo vazio/fora da Texture2D, referências externa/nula/não resolvida, atlas, metadata inválida e prefixo truncado são rejeitados.
- Build e classID não validados são rejeitados.

### Resultado
- `python -m pytest -q tests/test_unity_serialized_file.py` — 83 passed, 80 subtests.
- `python -m pytest -q tests` — 262 passed, 7 skipped, 203 subtests.
- `python -m py_compile tools/unity_serialized_file.py tests/test_unity_serialized_file.py` — passou.
- Quatro SerializedFiles locais: 2 Sprites e 2 regiões diretas validadas contra Texture2D local; nenhum identificador foi emitido.
- Argus APPROVE; Tifa APPROVE; cinco critérios Thoth PASS.

## Cenários da Tifa

```gherkin
Feature: Criar pacotes PC2PAK determinísticos

  Scenario: Pacote ordenado e reprodutível
    Given um manifest válido com assets em diretórios diferentes
    When o empacotador grava dois pacotes
    Then os bytes dos pacotes são idênticos
    And o índice fica ordenado por caminho UTF-8
    And offsets, tamanhos e CRC-32 correspondem aos payloads

  Scenario: Asset alterado desde a criação do manifest
    Given um pacote anterior e um asset cujo conteúdo mudou
    When o empacotador valida o manifest
    Then a operação falha sem substituir o pacote anterior
    And os arquivos temporários são removidos

  Scenario: JSON duplicado, path inválido e saída dentro da origem
    Given um manifest com chave JSON duplicada ou uma saída sob a raiz
    When o empacotador valida a entrada
    Then ele rejeita a operação antes de publicar um pacote

  Scenario: Caminho de asset identificado como link ou reparse point
    Given um asset que o filesystem reporta como link
    When o empacotador resolve os assets do manifest
    Then rejeita o caminho antes de copiar o conteúdo

  Scenario: Falha durante a substituição atômica
    Given um pacote anterior e uma falha de replace
    When o empacotador tenta publicar o pacote novo
    Then o pacote anterior permanece intacto
    And os temporários são removidos
```

```gherkin
Feature: Ler um pacote PC2PAK como FileSystem read-only

  Scenario: Ler asset válido
    Given um pacote PC2PAK v1 válido
    When read_all solicita um path indexado dentro do limite
    Then os bytes são retornados após validar o CRC-32

  Scenario: Path ausente, inválido ou asset acima do limite
    Given um pacote válido e um buffer de saída preenchido
    When read_all recebe path ausente, traversal ou limite insuficiente
    Then retorna not-found, erro ou too-large
    And preserva o buffer de saída

  Scenario: Índice malformado ou payload corrompido
    Given uma cópia do pacote com path inválido, duplicado, offset sobreposto ou CRC divergente
    When o leitor constrói o índice ou lê o payload
    Then rejeita o pacote ou a leitura sem alterar a saída

  Scenario: Escrita e remoção
    Given um BundleFileSystem válido
    When write_atomic ou remove é chamado
    Then retorna erro sem modificar o pacote
```

```gherkin
Feature: Carregar textura do pacote sem extração

  Scenario: PNG empacotado, cache e reload
    Given um pacote de teste com PNG 1x1
    When TextureManager carrega, compartilha, descarrega e recarrega a textura
    Then dimensões e pixels RGBA correspondem ao PNG
    And os handles e a contagem de cache permanecem corretos
    And nenhum asset é extraído do pacote

  Scenario: Traversal na API de textura
    Given um TextureManager usando BundleFileSystem
    When load recebe um path com traversal
    Then retorna argumento inválido sem criar handle
```

## Codec PNG e geração de atlases

```gherkin
Feature: Codec PNG limitado para a pipeline de assets

  Scenario: Decodificar RGB com todos os filtros suportados
    Given um PNG não interlaçado de 8 bits RGB com filtros 0, 1, 2, 3 e 4
    When o decoder converte a imagem para RGBA
    Then cada canal RGB preserva os bytes de origem
    And o alpha de cada pixel é 255

  Scenario: Preservar transparência em RGBA e repetir a codificação
    Given uma imagem RGBA com alpha 0, parcial e 255
    When a imagem é codificada duas vezes e decodificada
    Then os dois PNGs são idênticos
    And todos os canais recuperados são iguais aos originais

  Scenario: Recusar PNG corrompido, fora do formato ou acima do limite
    Given CRC inválido, stream truncado, filtro inválido, interlace, profundidade ou tipo de cor não suportado
    When o decoder recebe a entrada
    Then retorna erro explícito sem publicar pixels
    And bytes após IEND também são rejeitados

Feature: Calcular layout determinístico de atlas

  Scenario: Posicionar sprites com padding dentro dos limites
    Given retângulos válidos e limites máximos de atlas
    When o empacotador calcula placements
    Then todos cabem com o padding pedido
    And nenhum par se sobrepõe

  Scenario: Repetir layout e rejeitar entrada impossível
    Given a mesma lista ordenada e um sprite maior que o atlas
    When o layout é calculado duas vezes e depois recebe o sprite impossível
    Then os dois resultados válidos são iguais
    And a entrada impossível falha sem placements parciais

  Scenario: Rejeitar dimensões ou IDs inválidos
    Given ID duplicado, dimensão zero, padding ou quantidade acima do limite
    When o empacotador valida a entrada
    Then nenhum layout utilizável é retornado

Feature: Construir atlas PNG por CLI

  Scenario: Compor manifesto local preservando alpha
    Given um manifesto versionado com dois PNGs válidos sob a raiz autorizada
    When a CLI constrói o atlas
    Then grava PNG e JSON com dimensões e retângulos correspondentes
    And os pixels preservam alpha
    And o JSON não contém caminhos absolutos da máquina

  Scenario: Reproduzir artefatos para o mesmo manifesto
    Given um manifesto e imagens que não mudaram
    When a CLI grava o mesmo atlas em dois diretórios vazios
    Then os bytes dos PNGs e JSONs são idênticos

  Scenario: Recusar path traversal, link ou imagem inválida sem sobrescrever
    Given saídas antigas e manifesto com path fora da raiz, link, PNG inválido ou limites excedidos
    When a CLI valida e tenta construir o atlas
    Then retorna código diferente de zero
    And as saídas antigas permanecem intactas
```

  Scenario: Rejeitar sprite reportado como reparse point
    Given o filesystem reporta o caminho do sprite como link ou reparse point
    When a CLI valida o manifesto
    Then retorna erro antes de publicar os arquivos
    And as saidas anteriores permanecem intactas

## Resultado — codec PNG — 2026-11-02

- `python -m unittest discover -s tests -p test_asset_png.py -q`: PASS, 9 testes.
- `python -m py_compile tools/asset_png.py tests/test_asset_png.py`: PASS.
- Interoperabilidade adicional: Pillow abriu um PNG 1×1 RGBA produzido pelo encoder e leu todos os canais corretamente.
- Validação usa imagens sintéticas; formatos fora do contrato RGB/RGBA 8-bit não interlaçado seguem recusados.

## Resultado — layout de atlas — 2026-11-02

- `python -m unittest discover -s tests -p test_atlas_layout.py -v`: PASS, 5 testes.
- Os testes verificam bounds, padding, não sobreposição, ordem dos IDs, repetibilidade, entrada vazia, dimensões impossíveis, duplicatas e limite de 1024 itens.
- O algoritmo não gira sprites e devolve placements somente depois de completar o layout.

## Resultado — CLI de atlas — 2026-11-02

- `python -m pytest -q tests/test_build_atlas.py`: PASS, 8 passed, 1 skipped, 4 subtests.
- `python -m pytest -q tests/test_asset_png.py tests/test_atlas_layout.py tests/test_build_atlas.py tests/test_asset_manifest.py tests/test_asset_pack.py`: PASS, 44 passed, 5 skipped, 35 subtests.
- `python -m py_compile tools/build_atlas.py tests/test_build_atlas.py`: PASS.
- A CLI real compôs PNG/JSON e preservou alpha; hashes de manifesto, fontes e saída conferem; nenhum path absoluto aparece no sidecar.
- Repetir em duas pastas produziu bytes idênticos. Paths fora da raiz, PNG inválido, JSON com chave duplicada, atlas que excede bounds e saída dentro da origem falharam sem alterar saídas anteriores.
- O teste de symlink real foi SKIP porque o Windows desta sessao nao permitiu cria-lo; traversal, containment e rejeicao de reparse point simulado passaram.

## Execução

Os cenários são executados pela suíte CTest `asset_package_pipeline`, que gera
o pacote de entrada com `tools/asset_pack.py` e invoca os dois executáveis C++
diretamente com esse pacote temporário. A suíte não usa assets comerciais.

## Resultado

CMake 3.31.6 configurou MSVC 19.44 com o padrao C++11 solicitado pelo projeto;
MSVC nao oferece modo estrito `/std:c++11`, portanto isso nao certifica esse
modo. Os alvos `pc2ps3_bundle_file_system_test`,
`pc2ps3_bundle_texture_integration_test` e `pc2ps3_core_test` compilaram em Debug.

- `ctest -R asset_package_pipeline`: PASS, 1/1; os 7 testes Python invocaram os
  dois leitores C++ no pacote gerado pelo empacotador.
- `ctest -R "asset_manifest_inventory|asset_package_pipeline"`: PASS, 2/2.
  O inventario executou 16 cenarios; 3 skips sao de criacao de links/reparse
  points reais, indisponivel no sandbox. A guarda do empacotador tambem foi
  exercitada por sinal de reparse simulado.
- `pc2ps3_core_test.exe`: PASS, exit code 0.

O build completo `ALL_BUILD` nao terminou nesta sessao; os alvos afetados foram
compilados e executados. A suite CTest integral permanece pendente.

## QA do decoder raw LZ4 de metadados

```gherkin
Feature: Decodificar bloco raw LZ4 com limite para metadados UnityFS

  Scenario: Literais e extensão de comprimento
    Given um bloco raw LZ4 com literais e comprimento estendido
    When o decoder recebe o tamanho esperado dentro do limite configurado
    Then retorna exatamente os bytes literais declarados

  Scenario: Referência sobreposta e sequências consecutivas
    Given um bloco com referência backward sobreposta e literais finais
    When o decoder processa a sequência de match
    Then replica os bytes anteriores e retorna o tamanho exato

  Scenario: Offset inválido, truncamento ou expansão além do limite
    Given um bloco com literal, offset ou extensão truncados, offset zero/fora da saída,
      ou saída maior que o teto
    When o decoder valida as sequências
    Then falha explicitamente sem expandir além do limite

  Scenario: Tamanho produzido menor que o declarado
    Given um bloco válido estruturalmente cuja saída fica abaixo do tamanho esperado
    When o decoder termina de ler a entrada
    Then rejeita a saída incompleta
```

Os cenários são executados pelo CTest `unityfs_lz4_metadata` usando a suíte
Python, sem ler a instalação do jogo nem decodificar assets comerciais.

### Resultado do decoder LZ4
- `python -m unittest discover -s tests -p test_lz4_metadata.py -v`: PASS, 7 testes.
- CTest `unityfs_lz4_metadata`: PASS, 1/1.
- Build C++ completo (`ALL_BUILD`): NOT_PROVEN; o MSBuild parou após `ZERO_CHECK`, sem atualizar o log ou emitir diagnóstico, e foi interrompido depois de não progredir.

## QA do diretório UnityFS v6

```gherkin
Feature: Ler o block-info UnityFS sem acessar blocos de assets

  Scenario: Metadata sem compressão no início do bundle
    Given um UnityFS v6 com tabela de blocos e nós válidos antes dos dados
    When o parser lê o block-info
    Then retorna MD5, blocos e caminhos relativos validados
    And solicita leituras apenas do cabeçalho e da faixa de block-info

  Scenario: Metadata LZ4 ou LZ4HC posicionada no fim
    Given um UnityFS v6 com flag de metadata no fim e um block-info comprimido
    When o parser busca o intervalo declarado
    Then descomprime somente os metadados e respeita os tamanhos declarados
    And não lê bytes dos blocos de assets entre cabeçalho e metadata

  Scenario: Tabelas malformadas ou caminhos fora do contrato
    Given contagens negativas/excessivas, ranges inválidos, path traversal ou duplicatas
    When o parser valida o block-info
    Then rejeita a entrada antes de retornar qualquer tabela parcial

  Scenario: Compressão ou flags não suportadas
    Given LZMA para block-info ou flags reservadas
    When o parser valida o cabeçalho e a tabela
    Then falha explicitamente sem ler blocos de assets
```

Os cenários são executados pelo CTest `unityfs_directory_info` com fixtures
sintéticas; não dependem da instalação Unity.

### Resultado do parser UnityFS
- Testes Python UnityFS: 13 executados, 1 skip herdado do teste de symlink no Windows; decoder LZ4: 7/7.
- CTest `unityfs_header|unityfs_lz4_metadata|unityfs_directory_info`: PASS, 3/3.
- Um bundle local Unity foi analisado somente até o block-info: flags 67, LZ4HC, 3 blocos, 1 nó e 111 bytes de metadata descomprimida. Nenhum caminho interno foi registrado neste relatório e nenhum bloco de asset foi lido.

## QA — CLI inspector UnityFS / SerializedFile v21 (ClickUp `86e3y7c2q`)

### Cenários Gherkin

```gherkin
Funcionalidade: Inspecionar em memória um node UnityFS como SerializedFile v21

  Cenário: bundle sintético válido gera resumo JSON limitado
    Dado um bundle UnityFS sintético com um node SerializedFile v21 válido
    E um índice explícito seleciona esse node
    Quando executo o CLI de inspeção
    Então o processo termina com código zero
    E stdout contém exatamente um documento JSON válido
    E stderr está vazio
    E as chaves JSON raiz são node_index, unity_version, serialized_file_version, target_platform, counts e class_id_histogram
    E o histograma resume as contagens de classID dos objetos
    E nenhum arquivo de node ou payload é criado

  Cenário: resumo não revela dados internos nem payload
    Dado que path interno, nome do node, userInformation e payload contêm sentinelas distintas
    Quando executo o CLI no node válido
    Então nenhuma sentinela aparece em stdout ou stderr
    E nenhum arquivo de exportação é criado

  Cenário: caminho em unity_version é sanitizado
    Dado um SerializedFile v21 sintético cujo unity_version contém C:\private\QA_VERSION
    Quando executo o CLI no node válido
    Então unity_version é null no JSON
    E o caminho não aparece em stdout nem stderr

  Esquema do Cenário: índice inválido falha sem JSON parcial
    Dado um bundle sintético válido com um node
    Quando executo o CLI com índice <indice>
    Então o processo termina com código não zero
    E stdout fica vazio
    E stderr não revela path interno, nome, userInformation ou payload
    E nenhum arquivo de payload é criado
    Exemplos:
      | indice            |
      | -1                |
      | 1 (fora do range) |

  Cenário: bundle UnityFS malformado falha sem JSON
    Dado um arquivo UnityFS com cabeçalho ou block-info truncado
    Quando executo o CLI para inspecioná-lo
    Então o processo termina com código não zero
    E stdout fica vazio
    E nenhum arquivo de payload é criado

  Cenário: SerializedFile fora da versão v21 falha sem JSON
    Dado um bundle válido cujo node contém SerializedFile v20
    Quando executo o CLI nesse node
    Então o processo termina com código não zero
    E stdout fica vazio
    E nenhum arquivo de payload é criado

  Cenário: caminho com reparse point é rejeitado antes da leitura
    Dado um arquivo bundle ou um ancestral marcado como reparse point
    Quando o CLI valida o caminho antes de abrir o bundle
    Então a inspeção falha com erro sanitizado
    E stdout fica vazio
    E nenhum arquivo de payload é criado

  Cenário: node acima do limite é rejeitado
    Dado um bundle sintético válido com node maior que MAX_INSPECTED_NODE_BYTES
    E o teste reduz o limite por mock sem alocar um node gigante
    Quando executo o ponto de entrada do CLI para esse índice
    Então o processo termina com código não zero
    E stdout fica vazio
    E nenhum arquivo de payload é criado
```

### Execução headless e evidências finais

A Tifa executou os cenários sem GUI. Os bundles e SerializedFiles de teste são sintéticos. O cenário de reparse point usa mock determinístico; o cenário de limite reduz o teto via mock para evitar uma alocação grande.

- `python -m unittest discover -s tests -p test_unityfs_serializedfile_inspect.py -v`: 8 testes passaram.
- `python -m unittest discover -s tests -v`: 193 testes passaram; 7 foram ignorados.
- O CLI inspecionou em memória o node 0 real do build Unity analisado. Resumo emitido:

```json
{"class_id_histogram":{"1":309,"114":36,"115":7,"142":1,"198":5,"199":5,"21":1,"212":234,"4":309,"95":30},"counts":{"external_references":16,"objects":937,"reference_types":0,"script_types":7,"types":16},"node_index":0,"serialized_file_version":21,"target_platform":19,"unity_version":"2019.4.4f1"}
```

A execução real terminou com código 0 e stderr vazio. O relatório não contém caminho do bundle, nomes/paths internos, `userInformation` ou payload, e nenhum arquivo foi exportado.

## SerializedFile v21 sem TypeTree (2026-11-03)

### Cenários Gherkin

```gherkin
Feature: inventariar metadata SerializedFile sem TypeTree

  Scenario: fixture v21 TypeTree-disabled
    Given um SerializedFile v21 sintético sem TypeTree
    When o parser recebe allow_type_tree_disabled=true
    Then ele lê as tabelas até metadata_end sem consumir bytes do payload

  Scenario: preservar o gate padrão
    Given o mesmo arquivo v21 sem TypeTree
    When o parser é chamado sem o modo explícito
    Then ele rejeita o arquivo sem alterar o comportamento TypeTree-enabled

  Scenario: quatro arquivos locais de um build Unity
    Given os quatro caminhos .assets do manifesto local
    When leio apenas header, preamble e tabelas metadata
    Then cada cursor coincide com metadata_end e nenhum payload de objeto é lido
```

### Execução e evidência

- `python -m unittest discover -s tests -p test_unity_serialized_file.py`: 59/59 PASS.
- `python -m unittest discover -s tests -p test_unityfs_serializedfile_inspect.py`: 10/10 PASS, incluindo nó de bundle e arquivo standalone sem TypeTree.
- `python -m py_compile` nos dois módulos Python e seus testes: PASS.
- Leitura local: quatro arquivos v21 little-endian, `has_type_tree=false`; 4.271 objetos no total; 87.104 bytes lidos, exatamente até os limites de metadata; 0 bytes de payload de objeto lidos. A tabela declara 4.205.196 bytes de payloads em objetos (maior objeto: 1.048.692 bytes), usados somente como estatística.
- CLI standalone executada nos quatro arquivos: 4/4 exit code 0, 4.271 objetos e 32 tipos; nenhum caminho de entrada ou string de `userInformation` apareceu na saída JSON.
- Histograma agregado `class_id`: 1:7, 4:5, 20:1, 21:8, 28:7, 48:32, 49:349, 109:3, 114:18, 115:3829, 128:2, 150:3, 200:1, 213:2, 222:1, 223:1, 224:2.
- Isso valida somente a leitura host do metadata. Não decodifica campos, exporta assets nem comprova build PS3/RPCS3.

## Texture2D sem TypeTree (2026-11-03)

```gherkin
Feature: leitura bounded do cabecalho Texture2D

  Scenario: decodifica apenas os quatro campos numericos suportados
    Given um SerializedFile v21 Unity 2019.4.4f1 plataforma 19 sem TypeTree
    And um objeto classID 28 com nome e bytes de imagem sentinela
    When executo o inspector com --texture-summary
    Then cada entrada texture2d contem somente width, height, complete_image_size e texture_format
    And o nome e os bytes de imagem sentinela nao aparecem na saida

  Scenario: nao consome os bytes de imagem
    Given um CountingStream com nome, cabecalho Texture2D e pixels sentinela
    When leio o prefixo de metadata
    Then somente o tamanho do nome, a flag base e os quatro inteiros sao lidos
    And nenhum byte da string do nome ou dos pixels e consumido

  Scenario: rejeita layout e faixa invalidos sem JSON parcial
    Given uma versao, plataforma ou classe nao suportada, ou prefixo truncado
    When solicito --texture-summary
    Then o processo falha sem imprimir resumo parcial, nome ou pixels
```

Execucao 2026-11-03: `python -m unittest discover -s tests` passou com 207 testes e 7 skips por indisponibilidade de permissao para criar symlinks no Windows. O CountingStream confirma 21 bytes de prefixo lidos por Texture2D e zero bytes da string do nome ou de pixels. Casos negativos cobrem nome acima do limite e comprimento negativo.

Inspecao metadata-only dos quatro `.assets` do build Unity analisado: 4.271 objetos, sete Texture2D e 147 bytes de prefixo lidos no total. Dimensoes numericas: 1024x1024 (2), 512x512, 1920x1080, 98x98 e dois objetos 0x0. Formatos numericos: 1 (3), 4 (2), 25 (1), 3 (1). Nenhum nome, path ID, caminho, pixel ou arquivo de saida foi emitido.

## Valores TypeTree em objeto SerializedFile (2026-11-03)

```gherkin
Feature: leitura limitada de valores TypeTree

  Scenario: decodifica escalares e strings nos dois endians
    Given um objeto sintético com escalares, string UTF-8 e arrays
    And um campo marcado para alinhamento de quatro bytes
    When decodifico width, height, enabled e label
    Then os valores little-endian e big-endian coincidem com a fixture

  Scenario: percorre TypelessData sem retornar bytes de imagem
    Given um objeto Texture2D com m_ImageData e campos numéricos selecionados
    When solicito width e height
    Then os inteiros selecionados são retornados
    And os bytes de imagem e o nome do objeto não aparecem no resultado

  Scenario: rejeita contagem, truncamento, UTF-8 ou tipo inválido
    Given uma array acima do limite, payload truncado, string inválida ou folha desconhecida
    When decodifico os campos solicitados
    Then a operação falha sem resultado parcial

  Scenario: o CLI mantém somente campos Texture2D permitidos
    Given um objeto sintético com caminho e pixels sentinela
    When solicito width, height, complete_image_size e texture_format
    Then o JSON contém somente os inteiros selecionados
    And caminho, m_Name e pixels sentinela estão ausentes
```

Execução: `python -m unittest discover -s tests -p test_unity_serialized_file.py -q` (68 testes) e `python -m unittest discover -s tests -p test_unityfs_serializedfile_inspect.py -q` (14 testes), ambos PASS. `python -m py_compile` dos quatro arquivos Python alterados também passou. Fixtures cobrem little/big endian, alinhamento, string UTF-8, array regular e `TypelessData`, limite de contagem, faixa truncada, tipo desconhecido, common strings fornecidas pelo chamador e contrato de saída sem nome/caminho/pixels.

Suíte completa: `python -m unittest discover -s tests -q` executou 212 testes, PASS, com 7 skips já conhecidos por permissões de symlink no Windows. CTest não pôde ser executado: `cmake` e `ctest` não estão no PATH nem nos caminhos usuais do Build Tools; este card não altera C++ e nenhuma validação de toolchain PS3/RPCS3 é alegada.

## Sprite para Texture2D — cenários Tifa

```gherkin
Feature: Resumir vínculos de textura dos Sprites sem ler pixels

  Scenario: Ler Sprite little-endian com strings alinhadas e parar após o PPtr
    Given um objeto Sprite v21 válido do build 2019.4.4f1 e plataforma 19
    And o objeto contém nome UTF-8, atlas tags e bytes sentinela após texture
    When leio SpriteRenderData.texture
    Then o resultado é somente o status do vínculo local
    And o stream para exatamente após o PPtr e não consome a sentinela

  Scenario: Classificar referências nos dois endians
    Given PPtrs locais, nulos, externos e locais sem Texture2D correspondente
    When leio fixtures little-endian e big-endian
    Then os status são linked, null, external e unresolved respectivamente

  Scenario: Rejeitar metadados Sprite inválidos ou fora dos limites
    Given build, plataforma ou classID não suportados, strings inválidas, truncamento ou limites excedidos
    When o leitor tenta alcançar o PPtr
    Then lança UnitySerializedFileError sem devolver status parcial

  Scenario: Emitir somente contagens agregadas na CLI
    Given um SerializedFile com Sprites ligados, nulos, externos e não resolvidos
    When executo --serialized-file PATH --sprite-link-summary
    Then o JSON contém apenas as contagens dos quatro status
    And nomes, tags, IDs de objeto, caminhos, malha e pixels não aparecem
```

Execução Tifa: os 74 testes de `test_unity_serialized_file.py` e 16 testes de `test_unityfs_serializedfile_inspect.py` passaram. A suíte completa executou 220 testes, PASS, com 7 skips de symlink no Windows. `py_compile` dos quatro módulos Python passou. O CLI também leu os quatro `.assets` reais do build Unity analisado: 2 Sprites, 2 vínculos locais e zero referências nulas, externas ou não resolvidas; a saída foi agregada.

## BC7 Texture2D decoder v22 - 2026-11-03

### Cenarios Tifa
- Cada modo observado (0, 1, 3, 4, 5 e 6) decodifica um bloco conhecido para 16 pixels RGBA8 exatos.
- Fixtures exercitam particoes, ancoras, P-bits, indices independentes, selecao de indices e rotacao dos modos 4/5.
- Textura 5x3 valida recorte de blocos 4x4; mip tail valido produz somente o nivel base.
- Prefixo invalido, modos 2/7, payload truncado/inconsistente, metadata, dimensao e limites falham sem resultado parcial.

### Resultado
- Focados: `python -m pytest -q tests/test_unity_texture_pixels.py tests/test_unity_texture2d_streams.py` — 27 passed.
- Regressao PC2PS3: `python -m pytest -q tests` — 258 passed, 7 skipped, 190 subtests passed.
- `python -m py_compile tools/unity_texture_pixels.py` — passou.
- Quatro SerializedFiles de um build Unity: 129.600 blocos; modo 0=1.936, 1=24.955, 2=0, 3=39.079, 4=3.231, 5=32.960, 6=27.439, 7=0; prefixos invalidos=0.
- Uma textura BC7 real de 1920x1080 foi decodificada para 8.294.400 bytes RGBA em memoria, sem gravar nem emitir pixels.
- Argus: APPROVE. Tifa: APPROVE. Thoth: 5/5 criterios PASS.
- A execucao irrestrita de `python -m pytest -q` tambem coleta `rea/bridge/mitmproxy/test_capture.py` e falha no Windows porque o modulo Unix `resource` nao existe; a suite PC2PS3 em `tests/` passou.

## Sprite RGBA crop v21 - 2026-11-03

### Cenarios Tifa
- Recortar regiao com origem top-left ou bottom-left e obter pixels exatos na ordem PNG top-left.
- Preservar dimensoes, canais e alpha com cores distintas.
- Rejeitar origem, tipo, buffer, bounds e limites invalidos sem saida parcial.

### Resultado
- `python -m pytest -q tests/test_unity_texture_pixels.py` — 19 passed, 25 subtests.
- `python -m pytest -q tests` — 264 passed, 7 skipped, 207 subtests.
- `python -m py_compile tools/unity_texture_pixels.py tests/test_unity_texture_pixels.py` — passou.
- Argus APPROVE; Tifa APPROVE; cinco criterios Thoth PASS.
- O chamador escolhe explicitamente a origem do buffer; a orientacao visual do payload real do build Unity analisado ainda nao foi validada.


## QA - Sprite PNG exporter v21 (2026-11-03)

### Cenarios Gherkin

Feature: exportar Sprite v21 para PNG e atlas
  Scenario: exportar um Sprite sintetico e gerar manifesto compativel
    Given um SerializedFile v21 sintetico com Sprite e Texture2D local
    When a origem do buffer e escolhida explicitamente
    Then a saida contem sprite_000000.png e manifesto schema 1 sem IDs internos
    And tools/build_atlas.py consome o manifesto e o PNG

  Scenario: rejeitar entrada ou destino invalido
    Given entrada truncada, indice invalido, stream indisponivel ou destino inseguro
    When o exportador e executado
    Then falha sem arquivos parciais e sem expor caminhos ou IDs

### Resultado

- python -m pytest -q tests/test_unity_sprite_export.py: 5 passed.
- python -m pytest -q tests: 269 passed, 7 skipped, 207 subtests.
- python -m py_compile tools/unity_sprite_export.py tests/test_unity_sprite_export.py: passou.
- Fixtures validam PNG, manifesto, falhas sanitizadas, destino seguro e consumo pelo atlas builder.
- Smoke real do build Unity analisado pendente: os dois Sprites locais apontam para stream externo ausente; nenhum asset real foi exportado.
- Tifa APPROVE na QA; Argus APPROVE; Vincent APPROVE.

## Exportação em lote de Sprites v21

```gherkin
Feature: exportar um lote de Sprites sem publicação parcial
  Scenario: ordinais repetidos em ordem estável alimentam o atlas builder
    Given uma SerializedFile válida com dois Sprites e PNGs sintéticos
    When exporto os ordinais 1 e 0 em um lote
    Then a saída é ordenada por ordinal e contém nomes gerados e manifesto schema 1
    And o atlas builder aceita o manifesto e gera PNG e índice P2SA

  Scenario: ordinal inválido ou falha no segundo Sprite
    Given um lote com ordinal duplicado, inválido ou um Sprite que falha ao decodificar
    When exporto para um destino novo
    Then a validação rejeita o lote sem criar saída parcial ou revelar dados internos

  Scenario: limite agregado e retrocompatibilidade
    Given uma saída acima do teto agregado ou apenas um ordinal válido
    When exporto o lote
    Then o limite falha sem publicar arquivos e a CLI individual mantém o JSON anterior
```

Resultado: focado `test_unity_sprite_export.py`; confirmar que o manifesto em
lote também é aceito por `build_atlas.py`. A validação local registra apenas
dimensões e bytes PNG: os dois Sprites do `sharedassets0.assets` foram carregados
com o stream externo local; nenhum PNG de jogo foi escrito no repositório.

## Resultado — atlas runtime index v1 — 2026-11-03

### Cenarios Tifa

```gherkin
Feature: indice binario de atlas para runtime
  Scenario: serializar sprites validados em ordem deterministica
    Given um manifesto sintetico com IDs unicos e PNGs validos
    When o builder gera o atlas duas vezes
    Then cada saida contem PNG, JSON e indice v1 coerentes
    And os bytes do indice sao identicos e os campos numericos sao little-endian

  Scenario: falha durante a publicacao de sidecars
    Given PNG, JSON e indice anteriores ja publicados
    When a renomeacao do indice novo falha apos PNG e JSON
    Then os tres sidecars anteriores permanecem intactos
    And nenhum arquivo temporario permanece

  Scenario: falha sem sidecars anteriores
    Given o diretorio de saida esta vazio
    When a renomeacao do indice falha depois de PNG e JSON
    Then nenhum sidecar novo ou temporario permanece

  Scenario: entrada invalida ou fora dos limites
    Given IDs duplicados, paths nao canonicos ou retangulos invalidos
    When o builder valida o manifesto e o layout
    Then a operacao falha sem publicar saida parcial
```

### Evidencia

- `tests/test_build_atlas.py`: 10 passed, 1 skipped, 4 subtests passed.
- `tests/`: 271 passed, 7 skipped, 207 subtests passed.
- Os testes injetam falhas de publicacao com e sem sidecars anteriores, verificam
  restauracao/limpeza e ausencia de temporarios.
- `py_compile` dos arquivos Python do card passou.
- Revisoes de persona aplicadas localmente pelo Cid: Argus APPROVE, Vincent
  APPROVE; Tifa executou os cenarios via fixtures/CLI.
- ClickUp/board continua pendente por `BOARD_TOKEN_MISSING`.

## Resultado — atlas runtime index reader v1 — 2026-11-03

### Cenarios Tifa

```gherkin
Feature: ler o indice binario de atlas v1
  Scenario: decodificar indice valido e preservar a ordem
    Given bytes sinteticos com filename seguro e registros little-endian
    When o parser valida o indice
    Then publica dimensoes, filename e regioes na ordem codificada

  Scenario: rejeitar dados truncados, extras ou invalidos
    Given um indice com magic, versao, strings, ranges ou comprimento invalidos
    When o parser valida o indice
    Then falha sem alterar o resultado de saida
```

### Evidencia

- `pc2ps3_atlas_runtime_index_test` passou; `pc2ps3_texture_manager_test`
  tambem passou para a regressao da integracao seguinte.
- Build host completo com MSVC 14.44 passou.
- CTest completo: 70/70 passaram.
- Argus APPROVE e Tifa executou os cenarios; prompts aplicados localmente pelo
  Cid no perfil economico, portanto sem revisao independente por subagente.
- ClickUp/board pendente por `BOARD_TOKEN_MISSING`.
- `thoth-client verify` nao executou: o comando nao esta no PATH desta maquina.

## Resultado — atlas TextureManager integration v1 — 2026-11-03

### Cenarios Tifa

```gherkin
Feature: carregar atlas pelo TextureManager
  Scenario: carregar sidecar e PNG correspondentes com cache
    Given um FileSystem com indice v1 e PNG irma de dimensoes correspondentes
    When o TextureManager carrega, reutiliza e recarrega o atlas
    Then devolve handle valido e metadata sem reler a PNG enquanto em cache

  Scenario: rejeitar entradas e liberar lease em erro
    Given indice ausente, excessivo, malformado ou com filename inseguro
    When a PNG esta ausente ou diverge em dimensoes
    Then a chamada falha sem publicar metadata ou manter handle ativo

  Scenario: limitar a leitura e rejeitar handle preexistente
    Given um caminho logico inseguro ou um limite de bytes menor que o indice
    When a chamada recebe handle preexistente ou entrada acima do limite
    Then falha antes de ler caminhos derivados ou publicar resultado
```

### Evidencia

- `pc2ps3_texture_manager_test` passou com fixtures de caminho irmao, limite,
  cache/recarga, ausencias, traversal, divergencia de dimensoes e leases.
- `pc2ps3_atlas_runtime_index_test` passou como regressao do parser.
- Build host completo com MSVC 14.44 passou; CTest completo: 70/70 passaram.
- Argus APPROVE; Tifa executou os cenarios. Personas aplicadas localmente pelo
  Cid no perfil economico, sem revisao independente por subagente.
- ClickUp/board pendente por `BOARD_TOKEN_MISSING`.
- `thoth-client verify` nao executou: o comando nao esta no PATH desta maquina.
## Exportacao em lote de Sprites v21

```gherkin
Feature: exportar um lote de Sprites sem publicacao parcial
  Scenario: ordinais repetidos em ordem estavel alimentam o atlas builder
    Given uma SerializedFile valida com dois Sprites e PNGs sinteticos
    When exporto os ordinais 1 e 0 em um lote
    Then a saida e ordenada por ordinal e contem nomes gerados e manifesto schema 1
    And o atlas builder aceita o manifesto e gera PNG e indice P2SA

  Scenario: ordinal invalido ou falha no segundo Sprite
    Given um lote com ordinal duplicado, invalido ou um Sprite que falha ao decodificar
    When exporto para um destino novo
    Then a validacao rejeita o lote sem criar saida parcial ou revelar dados internos

  Scenario: limite agregado e retrocompatibilidade
    Given uma saida acima do teto agregado ou apenas um ordinal valido
    When exporto o lote
    Then o limite falha sem publicar arquivos e a CLI individual mantem o JSON anterior
```

Execucao: test_unity_sprite_export.py passou em 9/9 e py_compile passou para
modulo e testes. O manifesto em lote foi consumido pelo atlas builder, que gerou
PNG e indice P2SA. A validacao local decodificou em memoria dois Sprites do
sharedassets0.assets usando o .resS local: 1920x1080 e 98x98 (PNGs de
3.358.359 e 215 bytes). Nenhum PNG real foi salvo no repositorio.

## Exportacao em lote a partir de node UnityFS

```gherkin
Feature: exportar Sprites de um node UnityFS selecionado
  Scenario: selecionar um node entre varios sem materializar os demais
    Given um bundle sintetico com dois nodes em blocos separados
    When exporto um lote do node de indice 1
    Then o leitor solicita somente o indice selecionado e grava PNG/manifesto gerados
    And nenhum payload de node e escrito como arquivo intermediario

  Scenario: rejeitar node inexistente ou fonte invalida
    Given um bundle valido com indice fora do diretorio ou payload que nao e SerializedFile suportada
    When exporto os Sprites
    Then a operacao falha sem destino parcial e sem revelar caminhos internos

  Scenario: manter fontes CLI mutuamente exclusivas
    Given ambos --serialized-file e --bundle, ou --bundle sem --node-index
    When a CLI valida argumentos
    Then rejeita a combinacao antes de abrir arquivos ou publicar saida
```

Execucao: `test_unity_sprite_export.py`, `test_unityfs_data.py` e
`test_unityfs_serializedfile_inspect.py` passaram em 51/51; `py_compile` passou.
Fixture bundle leu o node 1 em blocos separados, publicou apenas PNG/manifesto
gerados e não escreveu node intermediário. O smoke Unity standalone permaneceu
compatível: dois Sprites decodificados e atlas PNG/JSON/P2SA gerados em diretório
temporário fora do repositório. Não houve smoke de Sprite real dentro de bundle,
nem execução PS3/RPCS3.

## TextureFormat 12 DXT5/BC3 - 2026-11-03

```gherkin
Feature: converter Texture2D DXT5/BC3 em RGBA8
  Scenario: ler endpoints e as duas tabelas de alpha
    Given um bloco DXT5 valido com endpoints RGB565 e indices conhecidos
    When converto a Texture2D para RGBA8
    Then cor e alpha correspondem aos indices do bloco

  Scenario: cortar blocos em dimensoes nao multiplas de quatro
    Given uma textura 5x3 com dois blocos DXT5 e mip tail consistente
    When decodifico o nivel base
    Then recebo 15 pixels na ordem raster e nenhum pixel fora da textura

  Scenario Outline: rejeitar payload inconsistente ou grande
    Given <entrada>
    When o decoder valida o payload DXT5
    Then falha antes de publicar pixels ou alocar acima dos limites
    Examples:
      | entrada |
      | bloco truncado |
      | tamanho completo divergente |
      | limite de pixels ou saida excedido |

  Scenario: empacotar todos os Sprites reais do node Unity confirmado
    Given 64 Sprites locais e o stream externo staged temporariamente
    When empacoto com --all-sprites e --check
    Then PNG, JSON e P2SA validam e os temporarios sao removidos
```
### Inventário bounded de AudioClip

- `tests/test_unityfs_serializedfile_inspect.py`: 47 testes e 3 subtests passaram após incluir a regressão AnimationClip. Inclui fonte standalone e UnityFS, classe ausente, TypeTree ausente, limites, opções incompatíveis, saída filtrada e determinismo.
- Smoke local por bundle index 1/node index 0: classID 83, 1 objeto, 180 bytes totais/máximos, uma definição TypeTree, 24 nós, profundidade 4 e 6 nós de tamanho variável.
- A saída de revisão contém apenas `node_index` e métricas agregadas; nenhum payload, nome de asset, path ou ID foi persistido. Nenhum binário do jogo foi executado.

### Inventário bounded de AnimationClip

- `tests/test_unityfs_serializedfile_inspect.py`: 47 testes e 3 subtests passaram; inclui árvore profunda de 524 nós, limite agregado, classe ausente, TypeTree desabilitado e regressão AudioClip.
- Smoke local bundle index 17/node index 0: classID 74, 13 objetos, 38.672 bytes no total, máximo 3.500, uma definição TypeTree, 524 nós, profundidade 9 e 111 nós de tamanho variável.
- A saída contém apenas `node_index` e métricas agregadas. Não leu nem emitiu curvas/keyframes, nomes, paths, IDs ou payloads. Nenhum binário do jogo foi executado.
- Suíte completa `tests/`: 334 passaram, 7 ignorados, 234 subtests passaram.
