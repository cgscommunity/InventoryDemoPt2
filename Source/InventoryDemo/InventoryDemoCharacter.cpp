// Copyright Epic Games, Inc. All Rights Reserved.

#include "InventoryDemoCharacter.h"
#include "Engine/LocalPlayer.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "GameFramework/Controller.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputActionValue.h"

#include "Item_Usable.h"
#include "InventoryComponent.h"

DEFINE_LOG_CATEGORY(LogTemplateCharacter);

//////////////////////////////////////////////////////////////////////////
// AInventoryDemoCharacter

AInventoryDemoCharacter::AInventoryDemoCharacter()
{
	// Set size for collision capsule
	GetCapsuleComponent()->InitCapsuleSize(42.f, 96.0f);
		
	// Don't rotate when the controller rotates. Let that just affect the camera.
	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = false;
	bUseControllerRotationRoll = false;

	// Configure character movement
	GetCharacterMovement()->bOrientRotationToMovement = true; // Character moves in the direction of input...	
	GetCharacterMovement()->RotationRate = FRotator(0.0f, 500.0f, 0.0f); // ...at this rotation rate

	// Note: For faster iteration times these variables, and many more, can be tweaked in the Character Blueprint
	// instead of recompiling to adjust them
	GetCharacterMovement()->JumpZVelocity = 700.f;
	GetCharacterMovement()->AirControl = 0.35f;
	GetCharacterMovement()->MaxWalkSpeed = 500.f;
	GetCharacterMovement()->MinAnalogWalkSpeed = 20.f;
	GetCharacterMovement()->BrakingDecelerationWalking = 2000.f;
	GetCharacterMovement()->BrakingDecelerationFalling = 1500.0f;

	// Create a camera boom (pulls in towards the player if there is a collision)
	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(RootComponent);
	CameraBoom->TargetArmLength = 400.0f; // The camera follows at this distance behind the character	
	CameraBoom->bUsePawnControlRotation = true; // Rotate the arm based on the controller

	// Create a follow camera
	FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
	FollowCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName); // Attach the camera to the end of the boom and let the boom adjust to match the controller orientation
	FollowCamera->bUsePawnControlRotation = false; // Camera does not rotate relative to arm


	InventoryComponent = CreateDefaultSubobject<UInventoryComponent>(TEXT("InventoryComponent"));


	// Note: The skeletal mesh and anim blueprint references on the Mesh component (inherited from Character) 
	// are set in the derived blueprint asset named ThirdPersonCharacter (to avoid direct content references in C++)
}

void AInventoryDemoCharacter::BeginPlay()
{
	// Call the base class  
	Super::BeginPlay();

	//Add Input Mapping Context
	if (APlayerController* PlayerController = Cast<APlayerController>(Controller))
	{
		if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PlayerController->GetLocalPlayer()))
		{
			Subsystem->AddMappingContext(DefaultMappingContext, 0);
		}
	}

	if (InventoryComponent)
	{
		InventoryComponent->ItemEquippedDelegate.BindUObject(this, &AInventoryDemoCharacter::InventoryItemAdded);
		InventoryComponent->ItemRemovedDelegate.BindUObject(this, &AInventoryDemoCharacter::InventoryItemRemoved);
	}
}

void AInventoryDemoCharacter::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (InventoryComponent)
	{
		InventoryComponent->ItemEquippedDelegate.Unbind();
		InventoryComponent->ItemRemovedDelegate.Unbind();
	}

	// Call the base class  
	Super::EndPlay(EndPlayReason);
}

//////////////////////////////////////////////////////////////////////////
// Input

void AInventoryDemoCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	// Set up action bindings
	if (UEnhancedInputComponent* EnhancedInputComponent = Cast<UEnhancedInputComponent>(PlayerInputComponent)) {
		
		// Jumping
		EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Started, this, &ACharacter::Jump);
		EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Completed, this, &ACharacter::StopJumping);

		// Moving
		EnhancedInputComponent->BindAction(MoveAction, ETriggerEvent::Triggered, this, &AInventoryDemoCharacter::Move);

		// Looking
		EnhancedInputComponent->BindAction(LookAction, ETriggerEvent::Triggered, this, &AInventoryDemoCharacter::Look);
	}
	else
	{
		UE_LOG(LogTemplateCharacter, Error, TEXT("'%s' Failed to find an Enhanced Input component! This template is built to use the Enhanced Input system. If you intend to use the legacy system, then you will need to update this C++ file."), *GetNameSafe(this));
	}
}

void AInventoryDemoCharacter::Move(const FInputActionValue& Value)
{
	// input is a Vector2D
	FVector2D MovementVector = Value.Get<FVector2D>();

	if (Controller != nullptr)
	{
		// find out which way is forward
		const FRotator Rotation = Controller->GetControlRotation();
		const FRotator YawRotation(0, Rotation.Yaw, 0);

		// get forward vector
		const FVector ForwardDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X);
	
		// get right vector 
		const FVector RightDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y);

		// add movement 
		AddMovementInput(ForwardDirection, MovementVector.Y);
		AddMovementInput(RightDirection, MovementVector.X);
	}
}

void AInventoryDemoCharacter::Look(const FInputActionValue& Value)
{
	// input is a Vector2D
	FVector2D LookAxisVector = Value.Get<FVector2D>();

	if (Controller != nullptr)
	{
		// add yaw and pitch input to controller
		AddControllerYawInput(LookAxisVector.X);
		AddControllerPitchInput(LookAxisVector.Y);
	}
}

//Inventory
bool AInventoryDemoCharacter::AddInventoryItem(AItem_Usable* InItem)
{
	const int32 oldItemCount = InventoryComponent->GetNumItems();

	//option to display msg in log that inventory was being tried to be added but inventory is full
	if ( oldItemCount < static_cast<int32>(EInventorySlotType::Count) )
	{
		InventoryComponent->AddItem(InItem);
	}

	const int32 newItemCount = InventoryComponent->GetNumItems();
	return newItemCount > oldItemCount;
	
}

void AInventoryDemoCharacter::RemoveInventoryItem(AItem_Usable* InItem)
{

}


void AInventoryDemoCharacter::InventoryItemAdded(AItem_Usable* InItem)
{
	OnInventoryItemAdded(InItem);
}


void AInventoryDemoCharacter::InventoryItemRemoved(AItem_Usable* InItem)
{
	OnInventoryItemRemoved(InItem);
}

void AInventoryDemoCharacter::EquipItem(EInventorySlotType Slot)
{
	if (InventoryComponent->GetNumItems() > 0)
	{
		//if item is equipped then we don't do anything
		if(!EquippedItem)
		{
			//add functionality to select either primary or secondary
			//can also use different inputs
			switch (Slot)
			{
			case EInventorySlotType::Primary:
				EquippedItem = InventoryComponent->GetItems()[static_cast<int>(Slot)];
				break;
			case EInventorySlotType::Secondary:
				if (InventoryComponent->GetNumItems() > static_cast<int>(EInventorySlotType::Secondary))
				{
					EquippedItem = InventoryComponent->GetItems()[static_cast<int>(Slot)];
				}
				break;
			case EInventorySlotType::Count:
				EquippedItem = InventoryComponent->GetItems()[0];
				break;
			default:
				break;
			}
		}
	}
}

void AInventoryDemoCharacter::UnequipItem()
{

}


void AInventoryDemoCharacter::DropItem()
{

}


void AInventoryDemoCharacter::DrawItem()
{

}


void AInventoryDemoCharacter::HolsterItem()
{

}