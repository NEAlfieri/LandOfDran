--[[
	Effects

	The sounds and particles the Tier+Tactical weapons fire with, ported from the AudioProfile,
	ParticleData and ParticleEmitterData datablocks in the add-on's .cs files.

	Blockland's originals point at textures in its own base/data/particles folder, which isn't here,
	so each one uses the nearest of ours: the muzzle flash and the tracer trail both used "dot".

	Loaded from Weapon_Package_Tier1.lua.
]]

local folder = "Add-ons/Weapon_Package_Tier1/"

--The .wav files the package ships, under names the weapon files use. A file that isn't there logs
--an error and is skipped, and anything asking for that sound then just makes no noise
local sounds = {
	TTPistolFire = "pistol_fire.1.wav",
	TTPistolClick = "pistol_click.wav",
	TTSubmachinegunFire = "submachinegun_lowdamage.wav",
	TTShotgunFire = "pump_shotgun_fire.wav",
	TTShotgunReload = "pump_shotgun_reload.wav",
	TTSportRifleFire = "sport_rifle_fire.wav",
	TTMagazineOut = "magazine_out.wav",
	TTMagazineIn = "magazine_reload.wav",
	TTMagazine = "magazine.wav",
	TTSlide = "slide.wav",
	TTJam = "jam.wav",
	TTGeneralReload = "tactical_general_reload.wav",
	TTAmmoPickup = "ammoget.wav"
}

for name, file in pairs(sounds) do
	newSoundType(name, folder .. file)
end

--The flash out of the barrel. pistolTrailParticle in the originals: yellow going to white, gone in
--about an eighth of a second, and not lit so it glows rather than being shaded like smoke
addParticleType("TTMuzzleFlashParticle", {
	texture = "Assets/particles/dot.png",
	lit = false,
	useInvAlpha = false,
	color0 = {1.0, 1.0, 0.0, 1.0},
	color1 = {1.0, 1.0, 0.4, 1.0},
	color2 = {1.0, 1.0, 1.0, 0.6},
	color3 = {1.0, 1.0, 1.0, 0.0},
	size0 = 0.30, size1 = 0.22, size2 = 0.10, size3 = 0.0,
	time0 = 0, time1 = 0.35, time2 = 0.7, time3 = 1,
	drag = 3,
	gravity = {0, 0, 0},
	lifetimeMS = 80,
	spinSpeed = 10
})

addEmitterType("TTMuzzleFlashEmitter", {
	particles = "TTMuzzleFlashParticle",
	--A handful of sparks rather than a cloud: a flash is over before the next shot
	ejectionPeriodMS = 12,
	periodVarianceMS = 0,
	ejectionVelocity = 2,
	velocityVariance = 1,
	ejectionOffset = 0,
	thetaMin = 0,
	thetaMax = 60,
	lifetimeMS = 0
})

--Where a shot lands: a small grey puff, lit so it sits in the world like dust knocked loose
addParticleType("TTImpactParticle", {
	texture = "Assets/particles/cloud.png",
	lit = true,
	useInvAlpha = true,
	needsSorting = true,
	color0 = {0.7, 0.68, 0.62, 0.55},
	color1 = {0.7, 0.68, 0.62, 0.35},
	color2 = {0.7, 0.68, 0.62, 0.15},
	color3 = {0.7, 0.68, 0.62, 0.0},
	size0 = 0.15, size1 = 0.45, size2 = 0.8, size3 = 1.1,
	time0 = 0, time1 = 0.3, time2 = 0.65, time3 = 1,
	drag = 5,
	gravity = {0, 4, 0},
	lifetimeMS = 500,
	lifetimeVarianceMS = 150,
	spinSpeed = 20
})

addEmitterType("TTImpactEmitter", {
	particles = "TTImpactParticle",
	ejectionPeriodMS = 8,
	periodVarianceMS = 2,
	ejectionVelocity = 3.5,
	velocityVariance = 2,
	ejectionOffset = 0,
	thetaMin = 0,
	thetaMax = 80,
	lifetimeMS = 0
})
