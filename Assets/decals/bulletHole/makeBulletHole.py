"""
Makes the bullet hole decal's albedo (with alpha) and normal map from one height field.
Everything that shows sits inside the circle that touches the square's sides, since the game turns
the square at random and slides that circle onto the face of the brick it's on.
"""
import sys
import numpy as np
from PIL import Image

N = 256
out = sys.argv[1]
rng = np.random.default_rng(7)

def smoothstep(a, b, x):
    t = np.clip((x - a) / (b - a), 0, 1)
    return t * t * (3 - 2 * t)

#Supersampled, then shrunk, so the rim of the hole isn't jagged
S = N * 4
v, u = np.mgrid[0:S, 0:S]
x = (u + 0.5) / S * 2 - 1
y = (v + 0.5) / S * 2 - 1
r = np.sqrt(x * x + y * y)
theta = np.arctan2(y, x)

#A wobbly outline: a few low harmonics, a different set for the hole than for the dent around it
def wobble(harmonics, strength):
    total = np.zeros_like(theta)
    for k in harmonics:
        total += rng.uniform(0.5, 1.0) / k * np.sin(k * theta + rng.uniform(0, 6.28))
    return 1 + strength * total

holeR = r / wobble([2, 3, 5, 7], 0.16)
dentR = r / wobble([2, 3, 4, 6, 9], 0.12)

#Height: 0 is the brick's face. A pit punched through, a wall climbing out of it to a lip pushed up
#around it, and the lip settling back into the face
pit = -1.0 * (1 - smoothstep(0.17, 0.30, holeR))
lip = 0.22 * smoothstep(0.22, 0.34, dentR) * (1 - smoothstep(0.36, 0.62, dentR))
height = pit + lip

#Hairline cracks running out from the hole
cracks = np.zeros_like(r)
for _ in range(7):
    angle = rng.uniform(0, 6.28)
    length = rng.uniform(0.45, 0.72)
    bend = rng.uniform(-0.5, 0.5)
    across = np.abs(np.angle(np.exp(1j * (theta - angle - bend * (r - 0.25))))) * r
    line = (1 - smoothstep(0.004, 0.014, across)) * smoothstep(0.24, 0.30, r) * (1 - smoothstep(length - 0.12, length, r))
    cracks = np.maximum(cracks, line)
height -= 0.10 * cracks

#Fine chips in the lip so the highlight on it breaks up
grain = rng.normal(0, 1, (S // 8, S // 8))
grain = np.array(Image.fromarray(grain.astype(np.float32)).resize((S, S), Image.BICUBIC))
height += 0.025 * grain * smoothstep(0.2, 0.3, dentR) * (1 - smoothstep(0.4, 0.62, dentR))

def shrink(a):
    return a.reshape(N, 4, N, 4).mean(axis=(1, 3))

height = shrink(height)
holeR, dentR, cracks, rSmall = shrink(holeR), shrink(dentR), shrink(cracks), shrink(r)

#X along +u, Y along +v (down the image, which the game doesn't flip), Z out of the face
bump = 26.0
dv, du = np.gradient(height)
normal = np.dstack([-du * bump, -dv * bump, np.ones_like(height)])
normal /= np.linalg.norm(normal, axis=2, keepdims=True)
Image.fromarray(np.uint8(np.clip(normal * 0.5 + 0.5, 0, 1) * 255 + 0.5), "RGB").save(out + "/bulletHole_normal.png")

#Albedo is multiplied by the brick's color: black down the hole, scorched around its mouth, then the brick's own plastic
shade = 0.03 + 0.97 * smoothstep(0.22, 0.44, holeR)
shade *= 1 - 0.55 * cracks
alpha = 1 - smoothstep(0.50, 0.80, dentR)
alpha = np.maximum(alpha, cracks * (1 - smoothstep(0.7, 0.8, rSmall)))
alpha *= 1 - smoothstep(0.86, 0.97, rSmall)
rgba = np.dstack([shade, shade, shade, alpha])
Image.fromarray(np.uint8(np.clip(rgba, 0, 1) * 255 + 0.5), "RGBA").save(out + "/bulletHole_albedo.png")

#No light from the sky finds its way down the hole, and nothing down there is shiny. The lip is scraped a little smoother than the brick
occlusion = 0.02 + 0.98 * smoothstep(0.20, 0.42, holeR)
occlusion *= 1 - 0.5 * cracks
Image.fromarray(np.uint8(np.clip(occlusion, 0, 1) * 255 + 0.5), "L").save(out + "/bulletHole_ao.png")
rough = 1.0 - 0.55 * smoothstep(0.24, 0.36, holeR)
rough = np.maximum(rough, cracks)
Image.fromarray(np.uint8(np.clip(rough, 0, 1) * 255 + 0.5), "L").save(out + "/bulletHole_rough.png")
