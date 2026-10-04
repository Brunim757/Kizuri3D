# Material Texturizado (Albedo + Normal + ORM) — Fluxo Detalhado

**Baseado em:** `Kizuri_Engine_TDD.md`, `Kizuri_Engine_Roadmap.md` (Fase 4 — Pipeline de Assets) e na tarefa registrada em `Kizuri_Engine_AgentTasks_Fase0.md`.

**Por que mora na Fase 4, não na 5:** tecnicamente envolve shader de iluminação (Fase 5), mas conceitualmente é o pipeline de Assets (Fase 4) terminando o que já tinha prometido — o Roadmap da Fase 4 já cita "Material usando uma Textura" como exemplo de dependência rastreada, então o material referenciar textura é parte do que a Fase 4 define, mesmo a execução técnica tocando shaders de outra fase.

**Confirmado no código antes de desenhar este fluxo:** UV já vem do import (glTF `TEXCOORD_0`). `Texture2D`/`SamplerState` já funcionam em outros shaders (Bloom/SSAO/FSR). **Tangente não existe em lugar nenhum** — é a peça que falta pra normal map funcionar.

---

## 1. Princípio geral

- **Convenção ORM**, igual ao padrão glTF: uma textura só, R=Oclusão, G=Roughness, B=Metallic. Os assets de origem (glTF) já vêm organizados assim — não reempacota nada na importação.
- **Textura multiplica a cor fixa, não substitui**: `albedo final = textura × cor do material`. Dá flexibilidade artística (tingir uma textura sem precisar de variante nova) e é o padrão de mercado.
- **Slot vazio = comportamento idêntico ao material sem textura de hoje.** Regra inegociável — nenhum material já existente pode mudar de aparência só por essa feature existir.
- **Espaço de cor é a parte que mais costuma dar erro silencioso:** Albedo é **sRGB**. Normal e ORM são **lineares** (nunca sRGB — são mapas de dado, não cor). Trocar isso não quebra compilação, só deixa a textura "lavada"/escura sem avisar nada.

---

## 2. Fluxo de tangente (pré-requisito do normal map)

```
1. No import de mesh (MeshImporter.cpp), depois de ler posição/normal/UV/índices:
   a. Se o glTF já tem o atributo TANGENT: usa ele direto
   b. Se não tem (caso comum — muitos exports simples não incluem):
      gera via MikkTSpace (biblioteca padrão de mercado, mesma que o Blender
      usa) — NÃO implementar algoritmo de tangente na mão, pra bater
      exatamente com o jeito que os normal maps dos assets foram gerados
      na ferramenta de origem
   c. Tangente guardada com sinal de handedness (w = +1 ou -1), usado
      depois pra calcular a bitangente corretamente (bitangent = cross(
      normal, tangent) * handedness)

2. .kzmesh ganha um campo novo no schema FlatBuffers pra guardar a tangente
   junto com posição/normal/UV/índices que já existiam

3. Arquivos .kzmesh ANTIGOS (de antes desse campo existir), ao carregar:
   a. Detecta ausência do campo de tangente
   b. Recalcula via MikkTSpace no load (mesmo caminho do passo 1b)
   c. Não quebra, não pede reimport manual — mesma lógica de compatibilidade
      já usada pro campo Softness do Light (TDD/Roadmap Fase 5b)
```

---

## 3. Fluxo do componente Material

```
1. Material ganha 3 slots de textura, cada um opcional: Albedo, Normal, ORM
2. Slot vazio usa uma textura "neutra" fixa, carregada uma vez pelo motor
   (não é "if" espalhado pelo shader checando se tem textura):
   - Albedo vazio  -> branco 1x1 (não altera a cor fixa do material,
     já que albedo final = textura × cor fixa, e branco × cor = cor)
   - Normal vazio  -> normal "neutro" 1x1, cor (128,128,255) em 8 bits,
     que desempacotado vira (0,0,1) em tangent space = sem perturbação
   - ORM vazio     -> ORM neutro 1x1, (255, 128, 0) — oclusão sem efeito,
     roughness no valor médio que o material já define via valor fixo,
     metallic em 0 (o canal B só SOBRESCREVE o metallic fixo quando a
     textura existe de verdade)
3. Arrastar uma Textura do Asset Browser pro slot Albedo/Normal/ORM no
   Inspector atribui ela (com Undo, mesmo padrão já usado em Mesh/Light)
4. Persistência: Material grava os GUIDs das 3 texturas (ou "vazio") no
   .kzscene/.kzmesh, mesmo padrão de referência por GUID já usado pra
   Mesh (sobrevive a mover pasta, Fase 4 original)
```

---

## 4. Fluxo de renderização

```
GeometryVS.hlsl (vertex shader):
1. Recebe tangente (e sinal de handedness) do vertex buffer, junto do que
   já manda hoje (posição, normal, UV)
2. Passa tangente pro pixel shader, além do que já passava

GeometryPS.hlsl (pixel shader — hoje só usa Albedo.rgb fixo, nunca lê pin.uv):
1. Declara as 3 texturas: AlbedoTex, NormalTex, ORMTex + um SamplerState
2. Amostra as 3 usando pin.uv (que já existe no shader, só nunca foi usado)
3. Albedo final = AlbedoTex.Sample(uv).rgb × Albedo.rgb (cor fixa do material)
4. Normal:
   a. Monta a matriz TBN a partir de tangente (do vertex), bitangente
      (calculada: cross(normal, tangente) × handedness) e normal
   b. Desempacota o normal map: sample.xyz * 2 - 1 (de [0,1] pra [-1,1])
   c. Transforma o normal de tangent space pra world space via TBN
   d. Esse é o normal que vai pro G-Buffer, substituindo o normal "cru"
      do mesh que é usado hoje
5. ORM:
   a. R (oclusão) multiplica a oclusão final (combinando com SSAO, se a
      5d/SSAO já estiver ativa)
   b. G (roughness) sobrescreve o roughness fixo do material quando a
      textura ORM existe
   c. B (metallic) sobrescreve o metallic fixo do material quando a
      textura ORM existe
```

---

## 4.1 Formato de compressão (BC1/BC3/BC5) e a lacuna do ORM

Conferido no código real (`TextureImporter.cpp`): a escolha de compressão já é mais esperta que "tudo BC1" —
```
BC1  -> cor opaca sem alpha (caso comum, mais rápido)
BC3  -> cor com alpha
BC5  -> normal map (2 canais, X/Y — reconstrói Z no shader)
sRGB -> só aplicado quando NÃO é normal map (srgb = !normal)
```
Detecção de "é normal map" hoje é por **nome de arquivo** (`NameHintsNormal`: contém "normal", "_nrm", "_nor") **ou** uma flag manual `asNormal` que já existe no código pra forçar o tipo sem depender do nome.

**Lacuna real: não existe caminho equivalente pra ORM.** Sem correção, uma textura ORM cairia no caminho padrão de Albedo (BC1/BC3 **com sRGB**) — errado, porque ORM é dado, precisa ser linear.

**Correção: duas camadas de detecção, trabalhando juntas.**

```
Camada 1 — palpite automático pelo nome do arquivo (import time):
  Mesmo princípio do NameHintsNormal, só que pra ORM: procura padrões
  comuns de mercado no nome — "_orm", "_arm", "_ao", ou "_rough"+"_metal"
  juntos. É só um chute rápido pra já vir comprimido certo no caso comum,
  sem precisar de ação manual.

Camada 2 — o slot do Material é a fonte de verdade (quando o nome falha):
  Quando a textura é arrastada pro slot Albedo/Normal/ORM no Inspector
  (Seção 3), esse ato é inequívoco sobre o que a textura é — não precisa
  mais adivinhar pelo nome. Nesse momento, confere se ela já está
  comprimida certo pra esse slot; se não estiver (ex: nome genérico tipo
  "textura2.png" caiu em BC1/sRGB, mas foi atribuída ao slot ORM),
  reimporta/recomprime automaticamente com o formato certo — sem ação
  manual do usuário, o próprio ato de atribuir ao slot já corrige.

Flag manual explícita (asNormal/asORM) continua existindo como opção pra
quem já sabe o tipo da textura antes mesmo de ter um Material pra testar.
```

## 5. Espaço de cor — detalhe que decide se fica certo ou "errado mas compila"

```
Na hora de criar a textura na GPU (upload, Fase 4 original):
- Albedo: formato sRGB (ex: DXGI_FORMAT_R8G8B8A8_UNORM_SRGB) — a GPU
  converte sRGB->linear automaticamente na amostragem, porque imagem de
  cor (o que o olho vê num editor de imagem comum) é salva em sRGB
- Normal: formato LINEAR (ex: DXGI_FORMAT_R8G8B8A8_UNORM, sem _SRGB) —
  é um mapa de direção/dado, não teria sentido nenhum "corrigir gama"
  numa coordenada XYZ
- ORM: formato LINEAR, mesmo motivo do Normal — oclusão/roughness/
  metallic são valores de dado, não cor

Errar isso (ex: marcar Normal como sRGB por engano, copiando o padrão do
Albedo sem pensar) não dá erro de compilação nem crash — só deixa a
textura com aparência errada (lavada ou escura demais), o tipo de bug
que passa despercebido se não for testado visualmente de propósito.
```

---

## 6. "Pronto quando" (critério de aceite)

- Cubo com `brick.kztex` no slot Albedo mostra o padrão de tijolo de verdade na superfície — não mais cor fixa.
- Um material **sem nenhuma textura atribuída** continua com exatamente a mesma aparência de antes dessa feature existir (teste de regressão, não só feature nova funcionando).
- Com uma textura de normal map de teste no slot Normal: girar a luz ao redor do objeto muda o padrão de luz/sombra na superfície de forma condizente com o relevo do normal map — prova que a TBN/tangente está certa, não só que "aceita a textura mas ignora o conteúdo dela".
- Albedo não aparece lavado/claro demais nem escuro demais comparado à textura original vista num visualizador de imagem comum — prova que o sRGB está correto.
- `.kzmesh` antigo (de antes da tangente existir) carrega normal, sem pedir reimport manual.

---

## 7. Fora de escopo desta tarefa

- Filtragem anisotrópica (já é item separado, 5k do `Kizuri_Engine_Fase5_Graficos_Supremos.md`)
- Decals (sistema à parte — nenhum vestígio de código encontrado apesar de relato anterior de "já implementado"; confirmar com o agente antes de prosseguir com Decal)
- Emissive map (textura de emissão própria) — não fazia parte do pedido original, candidato a tarefa futura se for necessário
