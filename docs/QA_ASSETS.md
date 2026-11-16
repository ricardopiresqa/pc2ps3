# QA — Manifesto de assets e dependências

## Cenários Gherkin

```gherkin
Feature: Dependências explícitas no manifesto de assets

  Scenario: Registrar dependências declaradas em ordem determinística
    Given um inventário com animations/player.json e as texturas a.png e z.png
    And um mapa JSON que declara z.png e depois a.png como dependências
    When o scanner gera o manifesto v2
    Then o arquivo do jogador contém depends_on ordenado como a.png e z.png
    And os demais arquivos têm depends_on vazio

  Scenario: Rejeitar referência ausente sem gravar saída parcial
    Given um mapa que aponta para textures/missing.png fora do inventário
    When a CLI gera o manifesto
    Then a CLI termina com erro
    And o arquivo de saída não existe

  Scenario: Rejeitar ciclo de dependências
    Given dois assets que declaram dependências um do outro
    When o scanner valida o mapa
    Then a operação falha com erro de ciclo
    And nenhum manifesto parcial é gravado

  Scenario: Rejeitar mapas e caminhos inválidos
    Given um mapa com path traversal, chaves JSON duplicadas ou tamanho acima do limite
    When a CLI valida o mapa
    Then a operação falha sem gravar saída parcial

  Scenario: Rejeitar diretório pai symlinkado do mapa
    Given o arquivo do mapa está sob um diretório pai que é symlink ou reparse point
    When o scanner valida o caminho do mapa
    Then a operação falha antes de ler o conteúdo
```

## Execução da Tifa

- `python -m unittest discover -s tests -p test_asset_manifest.py`: 16 testes,
  13 passaram e 3 foram ignorados porque o sandbox Windows não permite criar
  symlinks.
- CLI com mapa válido: exit code 0, 3 entradas; `depends_on` ficou ordenado como
  `textures/a.png`, `textures/z.png`.
- CLI com referência ausente: exit code 1; o manifesto de saída não foi criado.
- CTest `asset_manifest_inventory` passou (1/1): 16 cenários, 13 passaram e 3
  foram ignorados porque este sandbox Windows não permite criar links reais.

## Revisão de código e segurança

- Argus encontrou que o mapa rejeitava arquivo symlinkado, mas não um diretório
  pai symlinkado. A validação agora percorre os componentes pais antes de abrir
  o mapa; a revisão final não encontrou achados bloqueantes.
- Vincent não encontrou risco adicional na validação de caminhos, tamanho,
  leitura ou gravação atômica.
- O ambiente não permite criar symlinks reais; o caso de diretório symlinkado
  ficou SKIP e o caminho de reparse foi exercitado com teste simulado.
