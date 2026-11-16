# Estrutura do PC2PS3

O prompt permite adaptar a árvore proposta à implementação. Este checkout mantém
o core portátil em `include/pc2ps3/` e `src/core/`, os exemplos host em `samples/`
e os comandos públicos já usados em `tools/`. As pastas de plataforma, jogo e
packaging separam as próximas integrações sem alegar que o backend PS3 esteja
pronto.

| Caminho | Responsabilidade | Estado |
| --- | --- | --- |
| `include/pc2ps3/`, `src/core/` | Interfaces e runtime portáteis | Implementado por módulos; ver `docs/STATUS.md` |
| `platform/pc/` | Adaptadores host | Estrutura documentada; backend SDL2 pendente |
| `platform/ps3/` | Adaptadores PSL1GHT/SDL2_PSL1GHT e acesso ao dispositivo | Estrutura documentada; toolchain e hardware pendentes |
| `games/original/` | Conteúdo e configuração do vertical slice autoral | Sem assets comerciais; código atual permanece em `samples/` |
| `assets/phase0/` | Assets originais do ambiente de exemplo | `ICON0.PNG` e `PIC1.PNG` autorais |
| `analysis/asset-inventory/` | Notas de formato, procedência e inventário | Documentação; não guardar cópias de jogos comerciais |
| `tools/asset/` | Documentação e futuras ferramentas de asset | CLI existente permanece em `tools/asset_manifest.py` |
| `packaging/` | Configuração e evidência de packaging PS3 | Esqueleto somente; nenhum ELF, SFO ou PKG produzido |
| `docker/` | Lock e imagem reproduzível da toolchain | Lock verificável ausente; ver `docs/PHASE0.md` |
| `build/`, `dist/` | Saídas geradas locais | Não são fonte nem evidência de toolchain por si só |

Os caminhos novos contêm apenas instruções de responsabilidade. A implementação
de dispositivo e o empacotamento só avançam depois de obter uma toolchain
verificável. `original-pc/` não existe: não copiamos executáveis ou assets
comerciais para este repositório.
