# Kizuri Physics — Lista de Implementação (Fase 6)

## 1. Integração Jolt

- [x] FetchContent Jolt (v5.0.0) no ThirdParty.cmake
- [x] Biblioteca KizuriPhysics (Engine/Physics)
- [x] Trace/Assert handlers do Jolt
- [x] Broadphase layers (estático/dinâmico) + filtros
- [x] TempAllocator (10MB)
- [x] Adaptador JobSystem Jolt → enkiTS (scheduler único, sem threads extras)
- [x] Factory + registro de tipos

## 2. Corpos Rígidos

- [x] Criação/destruição de corpos (static/dynamic/kinematic)
- [x] Massa customizada + inércia calculada
- [x] Atrito e restituição por corpo
- [x] Teleporte de estáticos/cinemáticos
- [x] Alvos de movimento cinemático (MoveKinematic por substep)
- [x] Mapa entidade → corpo com handles + geração

## 3. Colisores

- [x] Box
- [x] Sphere
- [x] Capsule
- [x] Mesh (triângulos, estático)

## 4. Simulação

- [x] Passo fixo 60Hz com acumulador (máx 4 substeps)
- [x] Interpolação posição/rotação passada ao render (anti-tremor)
- [x] Gravidade configurável
- [x] Velocidade de plataforma aplicada antes da colisão (MoveKinematic)

## 5. Queries

- [x] Raycast (posição, normal, corpo atingido)
- [x] Teste: queda livre (gravidade reduz Y)
- [x] Teste: caixa repousa no chão (altura + repouso)
- [x] Teste: raycast acerta o chão

## 6. Ragdoll / Joints

- [x] API de constraints (point, hinge)
- [x] Montagem de ragdoll a partir de caixas
- [x] Teste: ragdoll cai sem explodir (posições finitas)

## 7. Editor

- [x] Componente Rigidbody no ECS (segue padrão hasX)
- [x] Undo/Redo (Add/Set/Remove via EditQueue)
- [x] Inspector de Rigidbody (tipo, massa, colisor, tamanhos, atrito)
- [x] Serialização RIGIDBODY na cena
- [x] Wireframe de colisores no Viewport (box/sphere/capsule)
- [x] Toggle Simulate no viewport + passo por frame
- [x] Poses interpoladas usadas no render sem sujar a cena

## 8. Fora do escopo (v1)

- [ ] VehicleController (fora do nicho souls-like atual)
- [ ] Soft bodies
- [ ] Destruição com fratura (Jolt permite, decisão do jogo)
