# Backend PlayStation 3

O código PSL1GHT e SDL2_PSL1GHT deve ficar isolado do core portátil. Nenhum
backend de dispositivo é declarado pronto: faltam lock com procedência verificada,
compilador PPU, ELF auditável e hardware/emulador compatível. Não adicionar SDK,
firmware ou chaves ao repositório.
