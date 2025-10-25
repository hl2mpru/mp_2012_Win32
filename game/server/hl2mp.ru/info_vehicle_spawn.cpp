#include "cbase.h"
#include "vehicle_base.h"
#include "eventqueue.h"

class vehicleSpawn : public CServerOnlyPointEntity
{
public:
	DECLARE_CLASS( vehicleSpawn, CServerOnlyPointEntity );
	DECLARE_DATADESC();

	void Spawn( void );
	void TimerThink( void );
	void CreateVehicle( void );
	const char *m_vehiclescript;
	const char *m_szName;
	const char *m_model;
	int m_vehicletype;
	bool p_EnableGun;
	float m_fSpawnNext;
	EHANDLE m_hLastCar;
};

LINK_ENTITY_TO_CLASS( info_vehicle_spawn, vehicleSpawn );

BEGIN_DATADESC( vehicleSpawn )
	DEFINE_THINKFUNC( TimerThink ),
	DEFINE_KEYFIELD( m_vehiclescript, FIELD_STRING, "vehiclescript" ),
	DEFINE_KEYFIELD( m_szName, FIELD_STRING, "name" ),
	DEFINE_KEYFIELD( m_model, FIELD_STRING, "model" ),
	DEFINE_KEYFIELD( m_vehicletype, FIELD_INTEGER, "vehicletype" ),
	DEFINE_KEYFIELD( p_EnableGun, FIELD_BOOLEAN, "EnableGun" ),
END_DATADESC()

void vehicleSpawn::Spawn( void )
{
	SetThink( &vehicleSpawn::TimerThink );
	SetNextThink( gpGlobals->curtime + 0.5 );
	CreateVehicle();
}

const char *gpVehicleList[] = {
	"prop_vehicle_jeep",
	"prop_vehicle_airboat",
	"prop_vehicle_jalopy",
};

void vehicleSpawn::TimerThink( void )
{
	SetNextThink( gpGlobals->curtime );

	if( !m_hLastCar ) 
		CreateVehicle();

	CPropVehicleDriveable *pJeep = dynamic_cast< CPropVehicleDriveable * >( m_hLastCar->GetBaseEntity() );

	if( pJeep ) {
		CBasePlayer *pPlayer = ToBasePlayer( pJeep->GetDriver() );
		if( !pPlayer )
			return;
		
		//Удаляем старые машины этого игрока с таким классом
		CPropVehicleDriveable *pEnt = NULL;
		while ( ( pEnt = (CPropVehicleDriveable *)gEntList.FindEntityByClassname( pEnt, pJeep->GetClassname() ) ) != NULL )
		{
			if (pEnt->GetPlayerMP() && pEnt->GetPlayerMP() == pPlayer && pEnt->GetOwnerEntity() == this ) {
				//Проверим чтоб в машине не было никого(админы), если есть высаживаем.
				CBasePlayer *pDriver = ToBasePlayer( pEnt->GetDriver() );
				if( pDriver )
					 pDriver->LeaveVehicle();
				
				pEnt->SetPlayerMP( NULL );
				pEnt->SetOwnerEntity( NULL );
				g_EventQueue.AddEvent( pEnt, "Lock", 0.0f, NULL, NULL );
				//Растворим машину
				pEnt->Dissolve(NULL, gpGlobals->curtime + 5.0, false);
			}
		}

		pJeep->SetPlayerMP( pPlayer );
		m_hLastCar = NULL;
		SetNextThink( gpGlobals->curtime + 5.0 );
	}
}

void vehicleSpawn::CreateVehicle( void )
{
	Vector vecOrigin = GetAbsOrigin();
	QAngle vecAngles = GetAbsAngles();
	CBaseEntity *pJeep = CreateEntityByName(  gpVehicleList[m_vehicletype] );
	if( pJeep ) {
		pJeep->SetAbsOrigin( vecOrigin );
		pJeep->SetAbsAngles( vecAngles );
		pJeep->KeyValue( "model", m_model );
		pJeep->KeyValue( "solid", "6" );
		pJeep->KeyValue( "vehiclescript", m_vehiclescript );
		pJeep->KeyValue("EnableGun",p_EnableGun);
		pJeep->KeyValue("targetname", m_szName );
		DispatchSpawn( pJeep );
		pJeep->Activate();
		pJeep->SetOwnerEntity( this );
		m_hLastCar = pJeep;
	}
}