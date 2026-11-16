# ADR 0036 — Resumo agregado de Texture2D com streams carregados

## Contexto

O CLI já reportava estados inline, external e empty sem expor dados de imagem. O resolver `external_data_root` foi adicionado como opção explícita, mas ainda não havia um caminho de inspeção agregada para validar o uso nos SerializedFiles reais.

## Decisão

Adicionar `--external-data-root` somente para `--texture-payload-summary`. Sem essa flag, manter o resumo anterior metadata-only. Com uma raiz fornecida, carregar streams de forma bounded e reportar apenas contagens e tamanhos agregados por formato/armazenamento, incluindo `loaded`. Limitar o total lido por execução a 256 MiB.

Se qualquer stream falhar, não emitir JSON parcial; usar exit code de erro e mensagem sanitizada. Nunca incluir path, IDs, payload ou pixels.

## Consequências

- O inspector consegue validar streams externalizados sem exportar assets.
- O limite agregado evita leitura cumulativa excessiva em arquivos com muitos Texture2D.
- A raiz precisa ser absoluta e local; o leitor continua responsável pela contenção, limites e redaction.
- A saída continua utilizável como resumo técnico e pode alimentar a análise posterior de BC7.

## Verificação

Fixtures sintéticas verificam o resumo metadata-only e loaded, offset correto, ausência de path/pixels no JSON e erro sem stdout parcial para traversal e ranges inválidos.

Nos quatro `.assets` de um build Unity, executar o CLI com o diretório pai de cada SerializedFile carregou os dois streams externalizados (RGB24 e BC7) e manteve o resultado agregado sem caminhos ou pixels.
