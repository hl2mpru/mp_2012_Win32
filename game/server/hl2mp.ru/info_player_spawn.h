#include "cbase.h"

class CSpawnPoint : public CPointEntity
{
public:
	DECLARE_CLASS(CSpawnPoint, CPointEntity);
	DECLARE_DATADESC();

	int	m_iDisabled;
	bool m_bNoDisabled;
	void InputEnable( inputdata_t &inputdata );
	void InputDisable( inputdata_t &inputdata );
};