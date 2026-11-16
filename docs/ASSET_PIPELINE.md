# Pipeline de assets

## Inventário

`tools/asset_manifest.py ROOT --output FILE` gera um JSON local de arquivos autorizados. O scanner:

- exige um diretório raiz explícito e não segue symlinks, junctions/reparse points ou arquivos especiais;
- registra os itens ignorados e ordena caminhos relativos POSIX;
- calcula SHA-256 em blocos, inclui extensão normalizada, categoria e tamanho;
- falha se a árvore não puder ser percorrida ou se um arquivo mudar durante o hash;
- grava o manifest por arquivo temporário e replace atômico, somente fora da árvore inventariada.

Categorias por extensão são apenas indicativas (`image`, `audio`, `video`, `font`, `archive`, `executable` e `unknown`). Não são prova de formato, conteúdo, compatibilidade ou ausência de DRM. Nenhum byte de conteúdo é copiado para o projeto, interpretado, descriptografado, executado ou alterado.

## Procedência e licença

O manifest preserva o caminho da raiz e metadados para auditoria local. Ele não atribui licença nem confirma titularidade. A pessoa deve registrar a origem, autorização e licença de cada conjunto fora do manifest antes de redistribuir ou empacotar qualquer asset. Não inclua o JSON se os nomes de arquivos ou caminhos da instalação forem privados.

## Conversão futura

Esta etapa inventaria; ela não converte. Um conversor posterior deve consumir apenas formatos permitidos e conhecidos, escrever em diretório de saída separado, registrar ferramenta/versão/hash de entrada e saída, e rejeitar DRM ou formato não suportado em vez de tentar contorná-lo. Não há assets comerciais de jogos comerciais neste checkout.

## Testes

`python -m unittest discover -s tests -p test_asset_manifest.py` cobre ordem, hashes, categorias, links, raiz/saída inválidas, alteração durante leitura, replace falho, mapa de dependências, ordenação, referências ausentes, caminhos inválidos, diretório pai symlinkado e ciclos. O CTest `asset_manifest_inventory` é registrado quando `PC2PS3_BUILD_TESTS` e `PC2PS3_BUILD_TOOLS` estão ligados.

## Dependências explícitas

O manifesto v2 inclui `depends_on` para cada arquivo. As relações vêm de um mapa JSON v1 separado; o scanner não infere dependências a partir de binários nem de assets comerciais. Exemplo:

```json
{
  "schema_version": 1,
  "dependencies": [
    {
      "asset": "animations/player.json",
      "depends_on": ["textures/player.png"]
    }
  ]
}
```

Execute `python tools/asset_manifest.py ROOT --dependency-map MAP.json --output manifest.json`. O mapa deve ficar fora da raiz inventariada. Caminhos são relativos POSIX e precisam corresponder a arquivos do inventário. A CLI rejeita referências ausentes, paths não canônicos, duplicatas, auto-dependências e ciclos; também rejeita mapa acima de 1 MiB, JSON com chaves duplicadas e symlink/reparse point no arquivo ou em qualquer diretório pai. As listas `depends_on` são ordenadas para manter o manifesto determinístico, e o arquivo final continua sendo gravado atomicamente.

Os cenários e a evidência desta entrega estão em [QA_ASSETS.md](QA_ASSETS.md).

## Exclusões canônicas
`tools/asset_manifest.py ROOT --exclude RELATIVE/PATH -o OUT.json` aceita
exclusões relativas POSIX canônicas. Uma exclusão de diretório cobre a árvore
inteira e é aplicada antes de abrir ou calcular hash de qualquer entrada nela;
traversal, caminhos absolutos e separadores invertidos são rejeitados. O match é
case-insensitive para manter o mesmo comportamento no host Windows e em QA
portátil. Chamadas sem `--exclude` preservam o manifesto v2 existente.

## Codec PNG e atlas offline
`tools/asset_png.py` é o codec local sem dependências externas para a etapa de
conversão. O contrato aceito é PNG 8-bit RGB/RGBA, não interlaçado, com filtros
PNG 0–4 e chunks IHDR/IDAT/IEND. Outros tipos de cor, metadados auxiliares e
compressões fora desse contrato são recusados. O decoder limita entrada a
64 MiB, dimensões a 4096 por eixo e pixels a 8.388.608; a saída do encoder é
RGBA. A ferramenta de atlas usa somente PNGs permitidos sob uma raiz explícita,
não copia assets comerciais para o projeto e não tenta contornar DRM.

`tools/atlas_layout.py` usa shelf packing estável por altura/largura decrescentes
e ID UTF-8, sem rotação. O contrato limita a 1024 sprites, 4096 pixels por eixo
e padding de 0 a 16 pixels. Os placements retornam na ordem do manifesto e o
retângulo do atlas contém somente a área usada mais o padding externo.

`tools/build_atlas.py ROOT MANIFEST --output-dir OUT [--name atlas]` aceita um
manifesto local `{"schema_version":1,"sprites":[{"id":"player-0",
"path":"sprites/player-0.png"}]}`. O diretório de saída precisa existir e
ficar fora da raiz de origem. A CLI rejeita links/reparse points, paths não
canônicos, conteúdo fora do contrato e limites excedidos; códigos de saída são
0 para sucesso, 2 para entrada inválida e 3 para falha de I/O. A validação
completa acontece antes da publicação, que usa arquivos temporários no destino.

O sidecar versão 1 registra ferramenta e versões de Python/zlib, hash do
manifesto, SHA-256 de cada PNG de origem e do PNG composto, IDs e retângulos.
Não inclui paths locais. A repetibilidade é garantida para a mesma entrada e o
mesmo toolchain Python/zlib; a mudança de versão fica visível no sidecar.


## Exportacao de Sprite v21

tools/unity_sprite_export.py combina o leitor de regiao Sprite, a leitura bounded de Texture2D, os decoders RGBA/BC7, o cropper e o codec PNG. O ordinal do Sprite e a origem das linhas sao explicitos; a saida e normalizada para linhas PNG top-left.

Exemplo:
python tools/unity_sprite_export.py --source-root C:/game --serialized-file assets/level0 --sprite-index 0 --source-origin bottom-left --output-dir C:/output/sprite-000000

O diretorio destino deve ser novo e ficar fora da raiz do projeto e dos assets de origem. Para streams externos, passe --external-data-root com uma raiz absoluta dentro da origem. A publicacao grava sprite_000000.png e manifest.json schema 1 em staging e renomeia o diretorio ao concluir; o manifesto usa ID ordinal e caminho relativo e pode ser consumido por tools/build_atlas.py.

A CLI emite apenas contagem e dimensoes. Erros nao repetem caminhos ou IDs, e nenhum pixel e impresso. A origem visual do payload real do build Unity analisado ainda precisa ser escolhida explicitamente; os testes do exportador usam fixtures sinteticas.

## Indice binario do atlas v1

`tools/build_atlas.py` tambem emite `<name>.p2sa` ao lado de `<name>.png` e
`<name>.json`. O indice usa magic `PC2SATL\0`, versao 1, dimensoes, contagem de
sprites e o filename relativo em little-endian. Cada registro inclui o ID UTF-8
e o retangulo na mesma ordem validada usada pelo PNG e pelo JSON. O builder
limita o indice pelos mesmos limites de sprites, texto, dimensoes e retangulos;
paths de origem, hashes e pixels nao entram no arquivo. A publicacao restaura
os sidecars existentes se uma renomeacao falhar.

O runtime valida o formato com `parse_atlas_runtime_index` sem acesso ao
filesystem. `TextureManager::load_atlas(index_path, handle, index)` le o
sidecar com limite `min(max_encoded_bytes, kMaxAtlasRuntimeIndexBytes)`, deriva
a PNG somente do diretorio logico do indice, carrega a imagem pelo decoder e
cache existentes e exige dimensoes iguais. Em erro, handle e metadata de saida
permanecem inalterados; uma textura carregada antes de uma divergencia libera o
lease temporario. Os cenarios C++ estao em
`tests/atlas_runtime_index_test.cpp` e `tests/texture_manager_test.cpp`.
