# How the Engine Works — A Full Breakdown

This covers every layer of the rendering pipeline, from the C code in `main.c` all the way down to the math inside the GPU shaders.

---

## The Big Picture

When you run the game, three things happen on every single frame:
1. **The CPU** (your C code) sets up *what* to draw and *where* the lights are
2. **The GPU Vertex Shader** transforms every point of every 3D model into screen space
3. **The GPU Fragment Shader** calculates the final colour of every pixel using PBR lighting math

These three stages form the **rendering pipeline** and run 60 times per second.

---

## Stage 1 — Startup (runs once)

### 1a. Window & Camera
```c
InitWindow(1500, 950, "...");

camera.position = (Vector3){2.0f, 2.0f, 6.0f};
camera.fovy = 45.0f;
camera.projection = CAMERA_PERSPECTIVE;
```
- `InitWindow` asks the OS for a window and an **OpenGL context** — a connection to the GPU.
- The **camera** is just a struct with a position, a look-target, and a field-of-view angle. It isn't magic — it's used later to build a matrix that transforms the 3D world into what the camera "sees."

---

### 1b. Loading the Shader Program
```c
Shader shader = LoadShader("resources/shaders/glsl330/pbr.vs",
                           "resources/shaders/glsl330/pbr.fs");
```
A **shader** is a small program that runs on the GPU. You always need two of them together:

| File | Runs on | Runs for each | Job |
|------|---------|--------------|-----|
| `pbr.vs` | GPU | Every **vertex** (corner point) of every model | Figure out *where* on screen a point lands |
| `pbr.fs` | GPU | Every **pixel** of every triangle | Figure out what *colour* that pixel should be |

Raylib compiles these text files and uploads the binary to the GPU. After this point, every `DrawModel` call will use this shader.

---

### 1c. Getting Uniform Locations
```c
int metallicValueLoc = GetShaderLocation(shader, "metallicValue");
int roughnessValueLoc = GetShaderLocation(shader, "roughnessValue");
int aoValueLoc = GetShaderLocation(shader, "aoValue");
```
**Uniforms** are variables you send from C (CPU) → the shader (GPU). They're called uniforms because they stay the same ("uniform") for every vertex/pixel during one draw call.

`GetShaderLocation` returns an integer ID — think of it as a mailbox number. Later you put data into that mailbox with `SetShaderValue`.

---

### 1d. Loading Models & Textures
```c
Model wall = LoadModel("resources/models/building_wall.glb");
car.materials[0].maps[MATERIAL_MAP_ALBEDO].texture = LoadTexture("resources/old_car_d.png");
```
- A **Model** is a collection of meshes (vertex data) + materials (how to shade it).
- A `.glb` file packages geometry + embedded textures + materials into one binary file.
- A **texture** is an image uploaded to the GPU's VRAM. The shader samples it using UV coordinates.

**Material map slots** tell raylib which texture goes where:

| Slot | What it stores |
|------|---------------|
| `MATERIAL_MAP_ALBEDO` | The base colour / diffuse texture |
| `MATERIAL_MAP_METALNESS` | MRA texture (Metallic, Roughness, AO packed into R,G,B channels) |
| `MATERIAL_MAP_NORMAL` | Normal map — encodes surface micro-detail as directions |
| `MATERIAL_MAP_EMISSION` | Emissive map — parts of the surface that glow |

---

### 1e. Creating Lights
```c
lights[0] = CreateLight(LIGHT_POINT, (Vector3){-2, 2, -3}, ..., YELLOW, 4.0f, shader);
```
A `Light` struct just holds position, colour, and intensity. `CreateLight` calls `GetShaderLocation` to find the matching `lights[0].position`, `lights[0].color`, etc. uniforms inside the shader. No GPU magic yet — it just remembers where to send the data.

---

## Stage 2 — The Game Loop (runs 60× per second)

```c
while (!WindowShouldClose())
{
    // 1. Update
    // 2. Draw
}
```

### 2a. Pushing Camera & Light Data to the GPU
```c
float cameraPos[3] = {camera.position.x, camera.position.y, camera.position.z};
SetShaderValue(shader, shader.locs[SHADER_LOC_VECTOR_VIEW], cameraPos, SHADER_UNIFORM_VEC3);

for (int i = 0; i < MAX_LIGHTS; i++)
    UpdateLight(shader, lights[i]);
```
This is the CPU stuffing data into the GPU's uniform mailboxes every frame. The shader needs the camera position to calculate specular highlights (shiny reflections depend on the viewer's angle).

---

### 2b. Per-Object Shader Setup Before Drawing
```c
// For the wall:
SetShaderValue(shader, aoValueLoc, &wallAo, SHADER_UNIFORM_FLOAT);
SetShaderValue(shader, metallicValueLoc, &wall.materials[0].maps[MATERIAL_MAP_METALNESS].value, ...);
SetShaderValue(shader, roughnessValueLoc, &wall.materials[0].maps[MATERIAL_MAP_ROUGHNESS].value, ...);
SetShaderValue(shader, GetShaderLocation(shader, "useTexMRA"), &noTex, SHADER_UNIFORM_INT);

DrawModel(wall, position, scale, WHITE);
```
Because all objects share the same shader, you have to re-configure it before drawing each one. `DrawModel` then:
1. Binds the material's textures to GPU texture slots
2. Sends the model/view/projection matrices as uniforms
3. Tells the GPU: *"run the vertex shader on every vertex, then the fragment shader on every resulting pixel"*

---

## Stage 3 — The Vertex Shader (`pbr.vs`) — Runs on GPU

```glsl
in vec3 vertexPosition;   // a single corner of a triangle, in model space
in vec2 vertexTexCoord;   // its UV coordinate (where to sample the texture)
in vec3 vertexNormal;     // the direction this surface faces
in vec4 vertexTangent;    // used to build the TBN matrix for normal maps

uniform mat4 mvp;         // Model × View × Projection matrix
uniform mat4 matModel;    // just the Model matrix
```

### What it does
```glsl
fragPosition = vec3(matModel * vec4(vertexPosition, 1.0));
```
**Model matrix** moves/rotates/scales the vertex from *model space* (as exported from a 3D editor) into *world space* (where everything in the scene lives together).

```glsl
gl_Position = mvp * vec4(vertexPosition, 1.0);
```
**MVP matrix** = Model × View × Projection. This is the magic that converts a 3D world position into a 2D screen position. Raylib computes and uploads this matrix automatically based on the camera.

### The TBN Matrix (for normal maps)
```glsl
TBN = transpose(mat3(fragTangent, fragBinormal, fragNormal));
```
A **normal map** stores surface normals in *tangent space* (relative to the surface itself). The TBN matrix is a rotation that converts those tangent-space normals into world-space normals the lighting math can use. Without it, normal maps would only work on flat surfaces facing one direction.

---

## Stage 4 — The Fragment Shader (`pbr.fs`) — Runs on GPU

This is where all the visual magic happens. It runs once for **every pixel** of every visible triangle.

### 4a. Reading Material Data
```glsl
vec3 albedo = vec3(1.0);
if (useTexAlbedo == 1) {
    albedo = texture(albedoMap, fragTexCoord * tiling + offset).rgb;
}
albedo = vec3(albedoColor.x*albedo.x, albedoColor.y*albedo.y, albedoColor.z*albedo.z);
```
- `texture(albedoMap, uv)` samples the albedo image at this pixel's UV coordinate — this is the base colour.
- It's then multiplied by `albedoColor` (the tint you pass from C, usually `WHITE` = no tint).

```glsl
if (useTexMRA == 1) {
    vec4 mra = texture(mraMap, ...);
    metallic  = mra.r + metallicValue;
    roughness = mra.g + roughnessValue;
    ao        = (mra.b + aoValue) * 0.5;
}
```
The MRA texture packs three values into one image:
- **Red channel** → metallic (0 = plastic/stone, 1 = bare metal)
- **Green channel** → roughness (0 = mirror, 1 = rough chalk)
- **Blue channel** → ambient occlusion (0 = fully shadowed crevice, 1 = open surface)

---

### 4b. The Surface Normal
```glsl
vec3 N = normalize(fragNormal);   // flat shading from geometry
if (useTexNormal == 1) {
    N = texture(normalMap, ...).rgb;
    N = normalize(N * 2.0 - 1.0);  // remap 0..1 → -1..1
    N = normalize(N * TBN);         // rotate into world space
}
```
`N` is the direction the surface is "facing" at this pixel. Normal maps let a flat polygon *pretend* it has fine surface detail (bumps, cracks, fabric weave) by perturbing `N` per-pixel. The actual geometry is unchanged; only the lighting direction changes.

---

### 4c. PBR Lighting Loop (Cook-Torrance BRDF)

**PBR** = Physically Based Rendering. The goal is to simulate how light *actually* behaves on different surfaces.

```glsl
vec3 baseRefl = mix(vec3(0.04), albedo.rgb, metallic);
```
All real-world surfaces reflect at least 4% of light (0.04). Metals use their own colour as the reflectivity base. `mix` blends between them based on how metallic the surface is.

```glsl
for (int i = 0; i < numOfLights; i++)
{
    vec3 L = normalize(lights[i].position - fragPosition); // direction TO the light
    vec3 H = normalize(V + L);                             // halfway between view and light
    float dist = length(lights[i].position - fragPosition);
    float attenuation = 1.0 / (dist * dist * 0.23);        // light falls off with distance²
    vec3 radiance = lights[i].color.rgb * lights[i].intensity * attenuation;
```

`attenuation` is the **inverse square law** — the same physics that makes a torch dim as you walk away. Doubling the distance quarters the brightness.

#### The Three BRDF Functions

```glsl
float D = GgxDistribution(nDotH, roughness);
```
**D — Normal Distribution Function**: How many micro-facets on the surface happen to be aligned exactly to reflect light toward the camera? High roughness = many misaligned facets = wide soft highlight. Low roughness = tight bright spec.

```glsl
float G = GeomSmith(nDotV, nDotL, roughness);
```
**G — Geometry / Self-Shadowing**: Micro-facets can block each other. Grazing angles (surface viewed nearly edge-on) have more self-shadowing. This prevents unrealistically bright edges.

```glsl
vec3 F = SchlickFresnel(hDotV, baseRefl);
```
**F — Fresnel**: At grazing angles, every surface becomes more mirror-like. Look at a table from the side — you'll see a reflection even if it's matte. `hDotV` is small at grazing angles, and Schlick's formula approximates the real Fresnel equations cheaply.

#### Combining It All
```glsl
vec3 spec = (D * G * F) / (4.0 * nDotV * nDotL);

vec3 kD = (vec3(1.0) - F) * (1.0 - metallic);   // metals have no diffuse
lightAccum += (kD * albedo / PI + spec) * radiance * nDotL * lights[i].enabled;
```
- `spec` = the specular (shiny) lobe
- `kD * albedo / PI` = the diffuse (scattered) contribution
- `nDotL` = Lambert's cosine law — a surface facing away from the light gets no light
- `lights[i].enabled` = multiplying by 0 or 1 to toggle lights on/off without branching

---

### 4d. Final Composition
```glsl
vec3 ambientFinal = (ambientColor + albedo) * ambient * 0.5;
return ambientFinal + lightAccum * ao + emissive;
```
- **Ambient**: Fake approximation of global scattered light so surfaces in shadow aren't pure black
- **lightAccum × ao**: All the point light contributions, dimmed in crevices by ambient occlusion
- **emissive**: Parts of the surface that glow regardless of lighting (headlights, displays, etc.)

```glsl
// HDR tonemapping
color = pow(color, color + vec3(1.0));
// Gamma correction
color = pow(color, vec3(1.0/2.2));
```
The PBR math produces values that can exceed 1.0 (HDR). **Tonemapping** compresses them back into the 0–1 display range. **Gamma correction** compensates for the fact that monitors are non-linear — without it, everything looks washed out.

---

## Data Flow Summary

```
main.c (CPU)
│
├─ LoadModel / LoadTexture     → uploads geometry + images to GPU VRAM
├─ LoadShader                  → compiles & uploads pbr.vs + pbr.fs to GPU
├─ SetShaderValue (per frame)  → sends camera pos, light data, metallic, roughness, ao...
│
└─ DrawModel ──────────────────────────────────────────────────┐
                                                               ↓
                                               GPU runs pbr.vs for every vertex
                                               → outputs: fragPosition, fragNormal, fragTexCoord, TBN
                                                               ↓
                                               GPU rasterizes triangles → generates pixels
                                                               ↓
                                               GPU runs pbr.fs for every pixel
                                               → reads textures, runs PBR math, outputs finalColor
                                                               ↓
                                                       Screen / Framebuffer
```
