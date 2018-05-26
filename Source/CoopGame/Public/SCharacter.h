// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "SCharacter.generated.h"

class UCameraComponent;
class USpringArmComponent;
class ASWeapon;

UCLASS()
class COOPGAME_API ASCharacter : public ACharacter
{
	GENERATED_BODY()

public:
	// Sets default values for this character's properties
	ASCharacter();

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

	void MoveForward(float value);

	void MoveRight(float value);

	void BeginCrouch();

	void EndCrouch();

	void BeginZoom();

	void EndZoom();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UCameraComponent* CameraComp;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	USpringArmComponent* SprinArmComp;

	//internal flag to notify if character is zooming or not in tick
	bool bWantsToZoom;

	float ZoomedFOV;

	UPROPERTY(EditDefaultsOnly, Category = "Player", meta =(ClampMin = 0.1, ClampMax = 100))
	float ZoomInterpSpeed;

	/*Default FOV set during begin play*/
	UPROPERTY(EditDefaultsOnly, Category = "Player")
	float DefaultFOV;

	ASWeapon* CurrentWeapon;

	UPROPERTY(EditDefaultsOnly, Category = "Player")
	TSubclassOf<ASWeapon> StarterWeaponClass;

	UPROPERTY(VisibleDefaultsOnly, Category = "Player" )
	FName WeaponAttachSocketName;

	void Fire();

public:	
	// Called every frame
	virtual void Tick(float DeltaTime) override;

	// Called to bind functionality to input
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

	/**
	* Override a la funcion de pawn para definir la vista del pawn en la camara y no en la cabeza del pawn.
	*/
	virtual FVector GetPawnViewLocation() const override;
};
