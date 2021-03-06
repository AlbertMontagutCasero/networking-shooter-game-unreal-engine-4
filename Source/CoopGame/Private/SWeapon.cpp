// Fill out your copyright notice in the Description page of Project Settings.

#include "SWeapon.h"
#include "DrawDebugHelpers.h"
#include "Kismet/GameplayStatics.h"
#include "Particles/ParticleSystem.h"
#include "Components/SkeletalMeshComponent.h"
#include "Particles/ParticleSystemComponent.h"
#include "PhysicalMaterials/PhysicalMaterial.h"
#include "TimerManager.h"
#include "CoopGame.h"
#include "Net/UnrealNetwork.h"

static int32 DebugWeaponDrawing = 0;
FAutoConsoleVariableRef CVARDebugWeaponDrawing(
	TEXT("COOP.DebugWeapons"),
	DebugWeaponDrawing,
	TEXT("Draw Debug Lines for Weapons"),
	ECVF_Cheat);

#define Out

// Sets default values
ASWeapon::ASWeapon()
{
	MeshComp = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("MeshComp"));
	RootComponent = MeshComp;

	MuzzleSocketName = "MuzzleSocket";
	TracerTargetName = "Target";

	RateOfFire = 600;

	SetReplicates(true);

	// LEARN Modifica la frequencia con la que se replica el actor (+ numero, mas refresco por segundo)
	NetUpdateFrequency = 66.0f;
	MinNetUpdateFrequency = 33.0f;
}

void ASWeapon::BeginPlay()
{
	Super::BeginPlay();

	TimeBetweenShots = 60 / RateOfFire;
}

void ASWeapon::Fire()
{
	//Trace the world, from pawn eyes to crosshair location.

	if (Role < ROLE_Authority)
	{
		ServerFire(); 
	}

	//Cogemos el propietario del arma (Especificado en la creacion del arma).
	AActor* MyOwner = GetOwner();
	if(MyOwner){
		FVector EyeLocation;
		FRotator EyeRotation;

		//funcion out que nos devolvera el punto de vista del propietario del arma
		MyOwner->GetActorEyesViewPoint(Out EyeLocation, Out EyeRotation);

		FVector ShotDirection = EyeRotation.Vector();
		FVector TraceEnd = EyeLocation + (ShotDirection * 10000);

		//Ajustando la collision para que no choque contra el mismo ni el propietario
		FCollisionQueryParams QueryParams;
		QueryParams.AddIgnoredActor(MyOwner);
		QueryParams.AddIgnoredActor(this);
		QueryParams.bTraceComplex = true;
		QueryParams.bReturnPhysicalMaterial = true;

		//Partile "Target" parameter
		FVector TracerEndPoint = TraceEnd;
		EPhysicalSurface SurfaceType = SurfaceType_Default;

		//Struct que contiene todos los datos del impacto
		FHitResult Hit;

		//El siguiente metodo traza una linea desde el punto de vista del propietario del arma hasta 10 metros en esa direccion.
		//Se le pasa el struct hit que sera llenado en caso de encontrar un actor con los parametros de condicion especificados
		//en el query params y retornara true si ha encontrado dicho actor.
		if (GetWorld()->LineTraceSingleByChannel(Hit, EyeLocation, TraceEnd, COLLISION_WEAPON, QueryParams))
		{
			//Blocking hit! Process Damage

			AActor* HitActor = Hit.GetActor();

			//Derreference weak object ,this save memory
			SurfaceType = UPhysicalMaterial::DetermineSurfaceType(Hit.PhysMaterial.Get());
			
			float ActualDamage = BaseDamage;
			if (SurfaceType == SURFACE_FLESHVULNERABLE)
			{
				ActualDamage *= 4.f;
			}

			UGameplayStatics::ApplyPointDamage(HitActor, ActualDamage, ShotDirection, Hit, MyOwner->GetInstigatorController(), this, DamageType);

			PlayImpactEffects(SurfaceType, Hit.ImpactPoint);

			TracerEndPoint = Hit.ImpactPoint;
		}

		if (DebugWeaponDrawing > 0)
		{
			DrawDebugLine(GetWorld(), EyeLocation, TraceEnd, FColor::White, false, TimeBetweenShots, 0, 1.0f);
		}

		PlayFireEffects(TracerEndPoint);

		if (Role == ROLE_Authority)
		{
			HitScanTrace.TraceTo = TracerEndPoint;
			HitScanTrace.SurfaceType = SurfaceType;
		}

		LastFiredTime = GetWorld()->TimeSeconds;
	}
}

void ASWeapon::OnRep_HistScanTrace()
{
	// Play cosmetic FX
	PlayFireEffects(HitScanTrace.TraceTo);

	PlayImpactEffects(HitScanTrace.SurfaceType, HitScanTrace.TraceTo);
}

void ASWeapon::ServerFire_Implementation()
{
	Fire();
}

bool ASWeapon::ServerFire_Validate()
{
	return true;
}

void ASWeapon::StartFire()
{
	float FirstDelay = FMath::Max(LastFiredTime + TimeBetweenShots - GetWorld()->TimeSeconds, 0.f);

	GetWorldTimerManager().SetTimer(TimerHandle_TimeBetweenShots, this, &ASWeapon::Fire, TimeBetweenShots, true, FirstDelay);
}

void ASWeapon::StopFire()
{
	GetWorldTimerManager().ClearTimer(TimerHandle_TimeBetweenShots);
}

void ASWeapon::PlayFireEffects(FVector TraceEnd)
{
	if (MuzzleEffect)
	{
		//Mostramos efecto en ca�on del arma
		UGameplayStatics::SpawnEmitterAttached(MuzzleEffect, MeshComp, MuzzleSocketName);
	}

	if (TracerEffect)
	{
		FVector MuzzleLocation = MeshComp->GetSocketLocation(MuzzleSocketName);
		UParticleSystemComponent* TracerComp = UGameplayStatics::SpawnEmitterAtLocation(GetWorld(), TracerEffect, MuzzleLocation);
		if (TracerComp)
		{
			TracerComp->SetVectorParameter(TracerTargetName, TraceEnd);
		}
	}
}

void ASWeapon::PlayImpactEffects(EPhysicalSurface SurfaceType, FVector ImpactPoint)
{
	UParticleSystem* SelectedEfect = nullptr;
	switch (SurfaceType)
	{
	case SURFACE_FLESHDEFAULT:
	case SURFACE_FLESHVULNERABLE:
		SelectedEfect = FleshImpactEffect;
		break;
	default:
		SelectedEfect = DefaultImpactEffect;
		break;
	}

	if (SelectedEfect)
	{
		FVector MuzzleLocation = MeshComp->GetSocketLocation(MuzzleSocketName);

		FVector ShotDirection = ImpactPoint - MuzzleLocation;
		ShotDirection.Normalize();

		//mostramos el efecto en el lugar de impacto
		UGameplayStatics::SpawnEmitterAtLocation(GetWorld(), SelectedEfect, ImpactPoint, ShotDirection.Rotation());
	}
}

void ASWeapon::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME_CONDITION(ASWeapon, HitScanTrace, COND_SkipOwner);
}
