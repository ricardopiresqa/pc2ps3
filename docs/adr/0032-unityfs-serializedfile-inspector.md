# ADR 0032: CLI de resumo para SerializedFile v21 dentro de UnityFS

- Status: Aceita
- Data: 2026-11-03

## Contexto

Precisamos classificar nodes UnityFS de uma instalação fornecida para a análise
do formato Unity sem despejar assets comerciais. Os leitores UnityFS e
SerializedFile já validam limites e leem apenas os blocos necessários para um
node selecionado.

## Decisão

Adicionar `tools/unityfs_serializedfile_inspect.py`, que exige `--bundle` e um
`--node-index` explícito. O CLI usa `parse_data_block_layout` e
`read_node_bytes`, mantém o node em memória e chama
`parse_serialized_file_metadata_v21`. O bundle precisa ser arquivo regular;
reparse points em qualquer componente do caminho são rejeitados. O limite do
leitor UnityFS se aplica ao tamanho do node. Somente SerializedFile v21 válidos
produzem saída.

A saída de sucesso é uma linha JSON com exatamente `node_index`,
`unity_version`, `serialized_file_version`, `target_platform`, `counts` e
`class_id_histogram`. O histograma conta os classIDs dos objetos. A versão Unity
só é mantida quando corresponde ao padrão seguro ASCII; valores fora dele viram
`null`. O JSON não inclui caminho do bundle, nome ou path interno do node,
`userInformation`, strings TypeTree nem bytes de payload. Nenhum arquivo é
criado. Em erro, o processo termina com código diferente de zero, mensagem
sanitizada em stderr e stdout vazio.

## Consequências

- A inspeção pode classificar um node sem escrever o asset ou expor os nomes
  armazenados no diretório UnityFS.
- O resumo serve para triagem estrutural; não é uma ferramenta de extração nem
  uma reconstrução dos assets.
- Os cenários sintéticos verificam sucesso, privacidade da saída, índices
  inválidos, bundle corrompido, versão não suportada e ausência de artefatos.
