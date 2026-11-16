# ADR 0035 — Leitura segura de streams externos Texture2D v21

## Contexto

Alguns registros Unity Texture2D armazenam `m_ImageData` fora do SerializedFile e apontam para bytes via `m_StreamData`. O parser existente só relatava o estado `external`, evitando abrir caminhos vindos de assets.

## Decisão

Manter o modo metadata-only como padrão. Um chamador pode passar explicitamente `external_data_root` local e absoluto para carregar um intervalo externo limitado em memória. Raízes UNC/rede são rejeitadas. O caminho precisa ser UTF-8 relativo e não pode conter URI, drive, segmento vazio, `.` ou `..`; componentes de symlink/junction são rejeitados e o destino resolvido precisa continuar dentro da raiz. Offset e tamanho são validados contra o tamanho real, com máximo de 64 MiB, antes da leitura exata.

Bytes carregados retornam como estado `loaded` sem guardar o caminho. O decoder possui uma entrada separada para esse estado e continua aceitando apenas Alpha8, RGB24 e RGBA32. Sem raiz, nenhum stream é aberto. BC7 permanece pendente.

## Consequências

- Metadados externos continuam seguros por padrão e não revelam caminhos.
- O caller escolhe uma raiz explícita; traversal, absolute paths, URIs, symlinks para fora, arquivos inexistentes e ranges inválidos falham sem leitura parcial.
- A leitura não extrai arquivos nem imprime bytes de imagem na CLI.

## Verificação

Fixtures sintéticas verificam offset exato, status sem resolução, caminho traversal/absolute/drive/URI/NUL/UTF-8 inválido, componentes de link e alvo resolvido fora da raiz, arquivo ausente ou faixa além do tamanho, e entrada do estado `loaded` no decoder RGBA.
