# Fase 3 — vertical slice original

## Protótipo host

`pc2ps3_prototype` é uma cena original de teste, sem assets comerciais. Liga
`World`, `StateMachine`, `TriggerSystem`, `CheckpointSystem`, `HitboxSystem`,
`EnemySystem`, `Camera2D`, `AnimationPlayer` e `Framebuffer`: o jogador corre,
ativa um checkpoint, executa um ataque, derrota um inimigo e chega ao estado de
vitória. O programa escreve `prototype.ppm` e imprime uma linha JSON com tick,
estado, posição, frame, inimigo, checkpoint e câmera.

Arte e valores de movimento pertencem somente a esta demonstração sintética; não
representam medições de jogos comerciais. O protótipo ainda não inclui AI de
inimigos, áudio, tela de resultado, backend SDL2/PSL1GHT ou fidelidade a um jogo
comercial.

## Validação disponível

O smoke target está registrado como `prototype_vertical_slice` no CTest. CMake e
CTest estão instalados com Visual Studio Build Tools, mas fora do `PATH`; use os
executáveis em `Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin`. O host
MSVC valida a build portátil e não comprova execução no RPCS3 ou em PS3 físico.

## Leitor UnityFS
`tools/unityfs_header.py` valida cabeçalhos UnityFS v6 e block-info em streaming.
`parse_directory_info` busca a tabela no início ou no fim segundo as flags, lê
somente o intervalo comprimido de metadados e aceita block-info sem compressão,
LZ4 ou LZ4HC. Contagens, tamanhos, flags, caminhos relativos e ranges são
limitados e validados antes de retornar metadados. `decompress_lz4_block` é
usado somente no block-info; nenhum bloco de assets é passado ao decoder.
O parser limita o block-info comprimido e descomprimido a 16 MiB, 65.536 blocos,
100.000 nós e 4 KiB por caminho.

`parse_data_block_layout` acrescenta os offsets físicos e lógicos dos blocos,
sem ler payload. O flag `0x200` só é tratado como alinhamento de 16 bytes para
revisões do Unity que o definem como padding (`2019.4.15`, `2020.3.34`,
`2021.3.2`, `2022.1.1` e linhas posteriores suportadas); revisões
antigas/ambíguas e a combinação com block-info no fim são rejeitadas, pois o
bit pode significar criptografia.

`tools/unityfs_data.py` lê um node selecionado usando esse mapa e descomprime
somente os blocos intersectados. O limite é 128 MiB por bloco e 256 MiB por
node; apenas none/LZ4/LZ4HC são aceitos. LZMA, criptografia e gravação em disco
ficam fora deste leitor. A separação está registrada em
[`0019-unityfs-node-reader.md`](adr/0019-unityfs-node-reader.md).

O decoder rejeita tipos e tamanhos inválidos, extensões e sequências truncadas,
offset zero ou fora da saída já produzida, e expansão diferente do tamanho
declarado. O limite absoluto é 128 MiB; consumidores podem impor um teto menor.

## Exportação local de nodes UnityFS

`tools/unityfs_extract_nodes.py` exporta nodes escolhidos por índice e grava nomes
gerados como `node_000001.bin`; paths internos do bundle nunca viram paths locais.
Exemplo:

```powershell
python tools/unityfs_extract_nodes.py --bundle .\sample.bundle --output-dir .\node-output --node-index 1 --node-index 4
```

A CLI mantém os limites do leitor, rejeita links/reparse points no caminho de
saída e não sobrescreve arquivos existentes. Os payloads são preparados em
temporários no diretório de destino e publicados atomicamente; erros removem os
temporários e qualquer arquivo desta seleção já publicado. Escolha um diretório
separado do repositório para evitar adicionar payloads de jogo aos fontes.
Cada chamada aceita até 64 índices e 256 MiB no total.

## Cabeçalho Unity SerializedFile

`tools/unity_serialized_file.py` valida somente o cabeçalho fixo de 20 bytes dos
formatos 9–21. Os campos fixos são big-endian; o marcador separado indica a
ordem dos metadados. O parser expõe os ranges do metadata e dos dados e limita o
arquivo declarado a 256 MiB. A instalação Unity 2019.4.4f1 examinada contém um
node formato 21 com tamanho 6.036, metadata de 5.077 bytes e `data_offset` 5.104.

TypeTree, tabela de objetos, referências PPtr e payloads ainda não são
interpretados. Formatos 22 ou posteriores falham explicitamente até haver uma
implementação e fixtures próprios.

`read_serialized_metadata_preamble` lê somente a string Unity version ASCII
(máximo 64 bytes), o target platform int32 e a flag TypeTree byte. As leituras
usam o endian do header e não podem cruzar o fim do metadata. TypeTree e a tabela
de objetos continuam sem parsing nesta etapa.

`parse_type_tree_blob` consome um blob v19–21 a partir do cursor fornecido. Ele
limita a 100.000 nodes e 1 MiB de string buffer, valida o stride fixo de 32 bytes
antes de ler os records e retorna offsets/nodes e o buffer bruto. Offsets locais
precisam estar dentro do buffer; offsets common-string (bit alto) são preservados
para resolução posterior. Object table e payload permanecem fora do leitor.
Referência estrutural: [AssetStudio SerializedFile.cs](https://github.com/Kappu12/AssetStudio/blob/main/AssetStudio/SerializedFile.cs).

`parse_serialized_type_table` lê a tabela após o metadata preamble somente em
SerializedFile v21 com TypeTree habilitada. A contagem usa int32 no endian do
header e é limitada a 10.000 tipos. Cada tipo preserva classID, flag `stripped`,
`scriptTypeIndex`, `oldTypeHash` bruto e o TypeTree do parser existente; `scriptID`
de 16 bytes é consumido somente para `classID` 114. `typeDependencies` usa int32
com limite por tipo de 100.000 e validação do range no metadata antes da leitura.

O limite agregado por tabela é 200.000 TypeTree nodes, 4 MiB de string buffers
e 250.000 dependências. O retorno inclui os tipos completos e o cursor exato após
a tabela, sem avançar para a object table. Truncamento, flag inválida, contagem
fora dos limites ou metadata insuficiente geram `UnitySerializedFileError`; a
função não retorna registros parciais.

`parse_serialized_object_table` começa exatamente no cursor após `SerializedType`
e lê somente v21 com TypeTree habilitada. O `object_count` int32 é limitado a
250.000. Cada registro alinha o stream a 4 bytes e lê `pathID` int64, `byteStart`
uint32 relativo a `data_offset`, `byteSize` uint32 e `typeID` int32; o classID é
obtido da tabela de tipos validada.

Cada ID de tipo e faixa de dados é validado contra a tabela e `file_size`. O
retorno contém registros completos e o cursor antes da script table seguinte. O
parser não lê payloads de objetos nem expõe resultados parciais.

`read_serialized_object_payload` recebe um header v21 validado e um registro
`UnitySerializedObject`. Resolve `data_offset + byte_start`, valida novamente o
limite global e a faixa em `file_size`, e retorna somente os `byte_size` bytes
selecionados em memória. Payload vazio retorna `b""`; ranges inválidos ou stream
truncado lançam `UnitySerializedFileError`, sem devolver dados parciais nem
gravar arquivos.

`parse_serialized_script_type_table` continua após o cursor validado da object
table e lê a tabela v21 com limite de contagem, `fileIndex` int32 e
`localIdentifierInFile` int64 alinhado a 4 bytes. O parser respeita endian e
`metadata_end` e devolve o cursor antes da tabela externa seguinte; não resolve
os índices de arquivo nem abre os caminhos associados.

`parse_serialized_external_references` lê a tabela seguinte com `assetPath`,
GUID bruto de 16 bytes, `type` int32 e `pathName`. As strings UTF-8 precisam de
NUL dentro de `metadata_end` e têm limite de 4096 bytes. O resultado preserva
os campos e o cursor antes de `refTypes`; os caminhos são dados e não são
abertos nem usados para escrita.
## Metadata v21 final (3 nov 2026)

Este estado consolida a cobertura atual dos parsers v21; as observações
anteriores de etapas parciais acima são históricas.

`parse_serialized_reference_type_table` continua após o cursor validado da
tabela de externals. Para cada refType, valida `classID`, flag `stripped`,
`scriptTypeIndex`, `scriptID` quando exigido pelo formato, `oldTypeHash`, o
TypeTree e os nomes de classe, namespace e assembly. As strings são limitadas e
terminadas em NUL dentro de `metadata_end`.

O parser limita a contagem de refTypes e os totais agregados de nodes e buffers
TypeTree. Depois da tabela, lê `userInformation` como string limitada e retorna
o cursor final. Truncamento, flags e contagens inválidos, TypeTree inconsistente
ou leitura além de `metadata_end` geram `UnitySerializedFileError`; listas
parciais não são retornadas. Nenhum caminho externo é aberto e nenhum payload é
lido ou escrito.

Os testes sintéticos cobrem v21, endians little/big, `scriptID` condicional,
TypeTree, campos de texto, tabela vazia, limites agregados, truncamento e
`userInformation`. A validação local executada nesta entrega passou nos 47
testes de `test_unity_serialized_file.py` e na suíte completa (177 testes; 7
skipped).

## Orquestração completa do metadata v21

`parse_serialized_file_metadata_v21(stream, available_size)` inicia no header e
compõe os parsers bounded existentes na ordem do formato: preâmbulo,
SerializedType, object table, script types, externals e refTypes com
`userInformation`. Aceita somente formato 21 com TypeTree habilitada e retorna
as tabelas juntas somente quando o cursor final coincide com `metadata_end`.
A operação não lê payloads de objetos nem resolve paths externos; erros internos
continuam como `UnitySerializedFileError` e não há resultado agregado parcial.
Veja [ADR 0030](adr/0030-unity-serializedfile-metadata-reader-v21.md).

## Resolução de strings do TypeTree

`resolve_type_tree_string_offset` resolve strings locais e common-string de
nodes TypeTree v19–21. Para common strings, o bit alto marca a referência e o
chamador fornece explicitamente o buffer comum; nenhuma tabela de terceiros é
embutida. Buffers são limitados, offsets precisam apontar para texto UTF-8 com
NUL em até 4096 bytes e erros usam `UnitySerializedFileError`.
`resolve_type_tree_node_strings` retorna `type_name` e `name` separadamente sem
modificar o node. Veja [ADR 0031](adr/0031-unity-typetree-string-resolution-v21.md).

## Inspeção de SerializedFile dentro de UnityFS

`tools/unityfs_serializedfile_inspect.py --bundle <arquivo> --node-index <n>`
lê um node selecionado em memória, valida-o como SerializedFile v21 e imprime
um único resumo JSON. A saída contém apenas `unity_version`, `target_platform`,
`node_index`, `serialized_file_version`, `counts` e `class_id_histogram`; versões
Unity com caracteres fora do padrão seguro viram `null`. O histograma conta
classIDs dos objetos. A entrada precisa ser arquivo regular e nenhum componente
do caminho pode ser reparse point; o limite de bytes do leitor UnityFS é aplicado
ao node selecionado. O JSON não inclui caminho do bundle, nome/path do node,
`userInformation` ou payload. Índices inválidos, bundles malformados e nodes
fora da versão suportada terminam com código diferente de zero e sem JSON
parcial. O leitor não cria arquivos. Veja
[ADR 0032](adr/0032-unityfs-serializedfile-inspector.md) e os cenários em
[QA Fase 3](QA_PHASE3.md).

Validação: 6 testes focados passaram; a suíte completa passou com 191 testes e
7 ignorados. O node 0 real do build Unity analisado foi inspecionado em memória e continha 937
objects, 16 types, 7 script types e 16 referências externas; nenhuma string
interna ou payload foi emitido.

## SerializedFile v21 sem TypeTree

Os quatro arquivos `.assets` de um build Unity reportam SerializedFile v21 little-endian com `has_type_tree=false`. O agregador agora oferece um modo explícito para esse formato: ignora os blocos TypeTree inexistentes, consome as tabelas metadata e valida o cursor até `metadata_end`, sem ler payloads dos objetos.

`tools/unityfs_serializedfile_inspect.py --serialized-file PATH` também inspeciona um `.assets` direto e imprime somente versão, plataforma, flag TypeTree, contagens e histograma de `class_id`. Na leitura local, os quatro arquivos somaram 4.271 objetos e 87.104 bytes de metadata lidos. O histograma agregado está em `docs/QA_PHASE3.md`; nomes e caminhos dos assets não foram persistidos.

## Cabecalho Texture2D sem TypeTree

`--texture-summary` e uma opcao explicita para arquivos standalone SerializedFile v21 de Unity 2019.4.4f1, plataforma 19, sem TypeTree. Ela le somente `width`, `height`, `complete_image_size` e `texture_format` de objetos classID 28. O nome alinhado e pulado com limite, e os bytes de imagem nao sao lidos; nomes, path IDs, caminhos e pixels nao entram no JSON. Outras versoes, plataformas ou layouts sao recusados.

O layout de heranca e campos foi conferido nas referencias publicas [Texture2D.cs](https://github.com/Perfare/AssetStudio/blob/master/AssetStudio/Classes/Texture2D.cs), [Texture.cs](https://github.com/Perfare/AssetStudio/blob/master/AssetStudio/Classes/Texture.cs), [NamedObject.cs](https://github.com/Perfare/AssetStudio/blob/master/AssetStudio/Classes/NamedObject.cs), [EditorExtension.cs](https://github.com/Perfare/AssetStudio/blob/master/AssetStudio/Classes/EditorExtension.cs) e [Object.cs](https://github.com/Perfare/AssetStudio/blob/master/AssetStudio/Classes/Object.cs). A implementacao nao copia codigo-fonte e limita o decoder ao build identificado nos arquivos do build Unity analisado.

## Leitor de valores TypeTree

`decode_type_tree_object_fields` decodifica campos escalares e strings UTF-8 de um objeto v21, respeitando endian, alinhamento TypeTree e o limite do payload. Arrays `Array` e `TypelessData` validam contagens e faixa antes de avancar; arrays byte podem ser ignorados sem copia ou emissao. Tipos folha desconhecidos, strings invalidas, truncamento e selecao ambigua falham sem resultado parcial.

O inspector oferece `--serialized-file PATH --object-index N --scalar-fields width height complete_image_size texture_format` para objetos Texture2D com TypeTree. O JSON usa somente esses quatro nomes canonicos e inteiros selecionados; nomes de objeto, path IDs, strings, caminhos e bytes de imagem nao sao emitidos. O objeto e lido em memoria sob o limite global do SerializedFile, mas nao e gravado ou retornado. TypeTree com offsets de common strings exige que o chamador da API forneca explicitamente `common_string_buffer`; a CLI nao inclui uma tabela comum embutida. Os `.assets` atuais do build Unity analisado nao possuem TypeTree e continuam usando `--texture-summary`.

## Links Sprite para Texture2D

`read_sprite_texture_link_status` aceita somente SerializedFile v21 do build Unity `2019.4.4f1`, plataforma 19, e objetos `Sprite` (classID 213). O leitor consome em etapas os campos de `SpriteRenderData.texture`, valida o nome e as atlas tags alinhadas sob limites e encerra a leitura imediatamente após o PPtr. Não lê a malha nem os dados de pixels do Sprite.

O PPtr é comparado em memória com os path IDs locais de objetos Texture2D (classID 28). A API devolve apenas `linked`, `null`, `external` ou `unresolved`; nunca devolve nomes, tags ou IDs. `--serialized-file PATH --sprite-link-summary` produz as contagens agregadas e não pode ser combinado com os outros resumos. Nos quatro `.assets` fornecidos do build Unity analisado, o resumo local encontrou dois Sprites, ambos ligados a Texture2D locais.

`read_sprite_region` lê o `m_Rect` de Sprite no mesmo layout validado e confirma que as coordenadas são finitas, integrais, positivas e cabem na Texture2D local vinculada. A região retorna apenas `(x, y, width, height)`; atlas, referências não locais e coordenadas ambíguas são rejeitados. Nenhum nome, GUID, PathID, caminho ou pixel é retornado. Nos arquivos de um build Unity, os dois Sprites passaram essa validação.

## Payloads Texture2D v21
`read_texture2d_image_data` lê em memória o array `m_ImageData` para o layout Unity de Unity `2019.4.4f1`, plataforma 19, SerializedFile v21 sem TypeTree. A leitura exige tamanho coerente com dimensões, mip count e TextureFormat; usa limite de 64 MiB por imagem e não escreve bytes em disco.
O resultado distingue `inline`, `external` e `empty`. Dados inline retornam bytes em memória; dados externos retornam somente o status e o tamanho declarado. O caminho e o offset do stream não são devolvidos nem resolvidos. A API aceita TextureFormat 1 (Alpha8), 3 (RGB24), 4 (RGBA32), 12 (DXT5/BC3) e 25 (BC7), que foram observados no inventário local.
`--texture-payload-summary` emite apenas contagens agregadas por formato e tipo de armazenamento. Nos quatro `.assets` locais, o resumo encontrou sete registros: três Alpha8 inline (2.359.296 bytes), um RGB24 externo (28.812 bytes), dois RGBA32 vazios e um BC7 externo (2.073.600 bytes). Nenhum nome, ID, caminho externo ou pixel foi emitido.

O reader aceita agora TextureFormat 12 (DXT5/BC3) quando o tamanho do payload confere com blocos de 4x4 e 16 bytes. O conversor produz RGBA8, incluindo as duas formas da tabela alpha e o recorte de blocos nas bordas; valida mip tail, dimensões, entrada e saída antes das alocações.

## Conversão de Texture2D para RGBA8
decode_texture2d_to_rgba converts Alpha8, RGB24, RGBA32, DXT5/BC3 and supported BC7 modes to RGBA8 in memory. The loaded-stream API uses the same converter; only the base level is returned. Pixels retain source order.
The decoder validates mip count and compressed block sizes before allocation, and enforces the existing 4096-dimension, 8,388,608-pixel and 32 MiB output limits. It rejects invalid prefixes, unsupported modes 2/7, truncation and inconsistent metadata without partial output.
## Streams externos de Texture2D
`read_texture2d_image_data(..., external_data_root=raiz)` pode carregar um stream externo para memória quando o chamador fornece explicitamente uma raiz absoluta local. Sem essa opção, o resultado continua `external` e nenhum arquivo é aberto. Caminhos são tratados como dados não confiáveis: somente caminhos UTF-8 relativos, sem URI, drive, segmento vazio, `.` ou `..`, são considerados; componentes de symlink/junction são rejeitados e o destino resolvido precisa permanecer dentro da raiz e ser arquivo regular. Raízes UNC/rede são rejeitadas.

Offset e tamanho são conferidos contra o arquivo antes de ler até 64 MiB. O resultado `loaded` contém os bytes, mas não o caminho; `decode_loaded_texture2d_to_rgba` encaminha os formatos Alpha8, RGB24 e RGBA32 ao conversor existente. Caminhos inválidos ou faixas truncadas falham sem retorno parcial e sem repetir o caminho na mensagem de erro. O modo padrão da CLI permanece metadata-only e não emite pixels.

O inspector aceita `--texture-payload-summary --external-data-root <diretório>` para usar essa resolução e incluir o estado `loaded` nos totais agregados por formato/armazenamento. Sem `--external-data-root`, mantém o resumo metadata-only. O total de streams lidos por execução é limitado a 256 MiB. O JSON não inclui path, ID, bytes ou pixels; se algum stream falhar, o processo termina com erro e não escreve JSON parcial.

Inspeção local dos quatro `.assets` do build Unity analisado, usando o diretório de cada SerializedFile como raiz, encontrou 7 Texture2D: três Alpha8 inline (2.359.296 bytes), dois RGBA32 vazios, um RGB24 loaded (28.812 bytes) e um BC7 loaded (2.073.600 bytes). O resumo não emitiu caminhos, IDs ou pixels.

`--bc7-mode-summary` exige também `--texture-payload-summary` e `--external-data-root`; conta os blocos BC7 de 16 bytes pelo prefixo unary no primeiro byte e separa prefixos inválidos. Não decodifica blocos nem retorna bytes/pixels. No BC7 local do build Unity analisado, os 129.600 blocos usam modo 0: 1.936; modo 1: 24.955; modo 3: 39.079; modo 4: 3.231; modo 5: 32.960; modo 6: 27.439; modos 2 e 7: zero; prefixos inválidos: zero.
\n\n## BC7 Texture2D decoding\nThe bounded RGBA8 decoder supports BC7 modes 0, 1, 3, 4, 5 and 6. Modes 4 and 5 use separate color/alpha index streams and channel rotation; multi-subset modes use the encoded partition and fix-up tables. The Unity histogram has no blocks in modes 2 or 7 and no invalid prefixes.\n

### BC7 v22 - validacao final

A regressao PC2PS3 passou com 258 testes, 7 skips e 190 subtests; o decoder passou em `py_compile`. A inspecao agregada dos quatro SerializedFiles locais encontrou 129.600 blocos nos seis modos observados e nenhum prefixo invalido. Uma textura BC7 real de 1920x1080 foi decodificada para 8.294.400 bytes RGBA em memoria, sem salvar nem emitir pixels.

## Recorte Sprite para RGBA8

`crop_sprite_rgba` recebe `UnityTexture2DRgba` e `UnitySpriteRegion` ja validados. O chamador precisa informar a origem das linhas do buffer (`top-left` ou `bottom-left`); o resultado e normalizado para ordem PNG top-left. A funcao valida tamanho exato do buffer, dimensoes, limites e bounds antes de alocar, preserva alpha e nao escreve arquivos. A orientacao dos bytes BC7 reais do build Unity analisado ainda nao foi validada visualmente.

## Exportacao em lote de Sprites v21

tools/unity_sprite_export.py aceita ate 64 ordinais unicos por execucao,
ordena os indices e publica PNGs de nomes gerados e manifesto schema 1 como
um diretorio unico. O total de PNGs e limitado a 128 MiB. Falhas limpam o
staging e deixam o destino sem publicacao. A API individual e o JSON de uma
exportacao continuam compativeis. Cenarios e evidencias estao em
[QA_PHASE3.md](QA_PHASE3.md) e no card local.

O modo bundle aceita `--bundle` e `--node-index`, usa o leitor UnityFS
existente para carregar somente o node escolhido (limite 256 MiB) e valida o
payload como SerializedFile v21 em memória. Nenhum arquivo de node
intermediário é criado. O destino continua sendo o pacote de PNGs/manifesto com
nomes gerados; streams externos passam pela mesma raiz local confinada.

## Vertical slice no PS3 (4 nov 2026)

`apps/ps3/main.cpp` é o executável do PKG (`PCPS00001`): o slice original jogável sobre
`World`, `TriggerSystem`/`CheckpointSystem`, `HitboxSystem`, `EnemySystem` (patrulha no
próprio app), `Camera2D` (fase de 1600 px) e `AnimationPlayer`. Controles: analógico/D-pad
corre, ✕ pula, □ ataca, Start pausa (e reinicia na tela de resultado após 1 s),
Start+Select sai. Tocar o inimigo vivo leva ao respawn no checkpoint ativo
(`World::respawn`). A fase termina no portal com tela de resultado (tempo, inimigo,
tentativas). Arte, áudio e valores são placeholders originais, não medições de jogos comerciais.

### Evidência RPCS3 0.0.43 + FW 4.91

- Sem atlas: `player sprite: original art (...)`, jogo e saída `exit status 0`;
  captura [img/ps3-rpcs3-slice.png](img/ps3-rpcs3-slice.png).
- Não comprovado: tela de resultado capturada em imagem (a injeção de teclas foi
  interrompida para não interferir no uso da máquina), D-pad, console físico.
