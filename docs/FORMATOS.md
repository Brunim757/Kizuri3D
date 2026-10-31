# Caligo — Formatos

Versão 0.1

## 1. Princípio

Formato é contrato. O runtime nunca lê formato de autoria. Toda fonte é texto ou formato aberto; todo pacote é binário, versionado e independente de plataforma. Um arquivo cozido não contém caminho absoluto, ponteiro, ordem de hash instável ou carimbo de tempo.

## 2. Identidade e hash

- **Asset GUID:** 128 bits aleatórios, escrito como 32 dígitos hexadecimais minúsculos. Nunca é reutilizado.
- **EntityId:** inteiro de 64 bits, único dentro da cena. `0` é inválido e representa ausência de pai.
- **Hash de fonte e de payload:** XXH64, little-endian, escrito como 16 dígitos hexadecimais.
- Referência entre assets é sempre por GUID. Referência entre entidades da mesma cena é por EntityId.
- Nome e caminho são metadados de edição; não identificam nada no runtime.

## 3. Árvore de conteúdo

```
assets/          fonte, versionada
scenes/          cenas de origem
scripts/         C# do jogo
cooked/          saída do cook, gerada
```

O cook é incremental: GUID, hash da fonte e versão do cozinhador decidem se um asset precisa ser refeito. O runtime recebe um manifesto que mapeia GUID para arquivo cozido.

## 4. Cena de origem

Extensão `.cscene`, UTF-8, JSON com esquema estável.

```json
{
  "format": "caligo.scene",
  "version": 1,
  "guid": "8f14e45fceea167a5a36dedd4bea2543",
  "entities": [
    {
      "id": 1,
      "name": "Player",
      "parent": 0,
      "components": [
        {
          "type": "caligo.transform",
          "position": [0.0, 0.0, 0.0],
          "rotation": [0.0, 0.0, 0.0, 1.0],
          "scale": [1.0, 1.0, 1.0]
        }
      ]
    }
  ]
}
```

Regras:

- `guid` identifica a cena; `id` identifica a entidade dentro dela.
- Todo componente tem `type` estável, nunca o nome de uma classe C++.
- Transform é obrigatório em toda entidade; ausência é erro de cook.
- Componentes desconhecidos preservam os dados e falham com diagnóstico, nunca são descartados em silêncio.
- A ordem das entidades não importa na origem; o cook ordena por `id`.
- Prefab é uma cena com uma raiz marcada e é instanciado no cook, não no runtime.

## 5. Contêiner cozido

Extensão `.casset`. Cabeçalho de 64 bytes, little-endian.

| Offset | Tamanho | Campo |
|---|---|---|
| 0 | 4 | magic `CALG` |
| 4 | 2 | versão maior |
| 6 | 2 | versão menor |
| 8 | 2 | tipo |
| 10 | 2 | flags |
| 12 | 4 | tamanho do payload |
| 16 | 8 | hash da fonte |
| 24 | 16 | GUID |
| 40 | 8 | hash do payload |
| 48 | 8 | reservado, zero |
| 56 | 8 | reservado, zero |

Tipos: `1` cena, `2` malha, `3` textura, `4` áudio, `5` material, `6` UI, `7` fonte, `8` script, `9` manifesto.

O payload começa no offset 64, alinhado a 16 bytes. Payload estruturado usa FlatBuffers com esquema versionado; payload bruto usa blocos com offset e tamanho explícitos. O runtime rejeita versão maior desconhecida e aceita versão menor quando o esquema garante compatibilidade.

## 6. Cena cozida

O payload de cena é organizado em tabelas paralelas, não em árvore de objetos:

- Tabela de entidades ordenada por EntityId: id, pai, intervalo de componentes, nome.
- Tabela de componentes por tipo, em arrays contíguos.
- Tabela de strings única, referenciada por offset e tamanho.
- Tabela de recursos por GUID, sem caminho de origem.
- Nenhuma referência por índice de array que dependa da ordem de autoria.

O carregamento é direto: o bloco vira memória imutável e as consultas usam os índices já ordenados.

## 7. Malha, textura e áudio

**Malha `.casset` tipo 2:** posição `float3`, normal `float3`, tangente `float4`, UV0 `float2`, cor `unorm8x4`; índices `uint16` ou `uint32`; até quatro LODs; bounds; slots de material por GUID. Otimização e geração de LOD acontecem no cook.

**Textura `.casset` tipo 3:** BC1, BC3, BC5 ou BC7; mipmaps completos; flag de sRGB; dimensões e tamanho de cada nível no payload. Formato de origem é convertido no cook, nunca no runtime.

**Áudio `.casset` tipo 4:** PCM 16 bits ou float 32; taxa, canais, quadros e tamanho do bloco. Música longa usa streaming por blocos de 64 KB; efeito curto é carregado inteiro.

## 8. UI de jogo

- `.cui` — documento de markup, UTF-8, estrutura declarativa descrita em `docs/sistemas/UI.md`.
- `.cuistyle` — folha de estilo, UTF-8, seletores por classe, id e estado.
- O cook valida o documento, resolve fontes e sprites e gera o payload tipo 6.
- Cada nó recebe um ID estável de 64 bits derivado do caminho no documento. Mudar a hierarquia muda o ID; por isso o editor mantém IDs explícitos quando o nó precisa sobreviver a uma reorganização.
- Nenhuma string de jogo fica literal no markup: toda string é chave de localização.

## 9. Script

- Assembly: `Caligo.Gameplay.dll`, .NET 10, Debug com `.pdb`.
- Tipo de entrada: `Caligo.Gameplay.GameModule`, que implementa `Caligo.Scripting.IGameModule`.
- O assembly referencia apenas `Caligo.Scripting.dll`; não referencia render, física ou memória.
- A superfície nativa é uma ABI C versionada, gerada a partir de um esquema. O lado C# usa wrappers tipados; o lado C++ nunca expõe STL, exceção ou ponteiro opaco sem contrato.
- Hot reload usa `AssemblyLoadContext` coletável. Estado estático é proibido no assembly de gameplay. O contexto anterior é descarregado após dois frames sem referência.
- Sem script, a engine inicia e roda a cena. Script é uma camada, não um requisito de boot.

## 10. Versionamento

- Versão maior: mudança incompatível. O runtime recusa o arquivo; o cook converte a fonte.
- Versão menor: adição compatível. O runtime ignora campos que não conhece apenas quando o esquema marca o campo como opcional.
- O cozinhador grava sempre a versão atual. O manifesto registra a versão de cada pacote.
- Nenhum campo é reaproveitado com outro significado. Campo removido vira reservado e permanece no esquema até a próxima versão maior.

## 11. Determinismo

- Toda gravação é little-endian e sem padding não inicializado.
- Ordem de tabelas, materiais, LODs e strings é estável e independente de hash de ponteiro.
- Floats são IEEE 754 de 32 bits. NaN e infinito são rejeitados no cook.
- O cook não grava data, hora, usuário, caminho absoluto ou nome da máquina.
- Dois cooks da mesma fonte produzem bytes idênticos.

## 12. Histórico

- **0.1** — formatos de cena, contêiner, malha, textura, áudio, UI e script.
