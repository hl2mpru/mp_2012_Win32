#include "cbase.h"
#include "filesystem.h"
//#include "sqlite3.h"
//#include "mysql.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

extern void Hack_FixEscapeChars( char *str );

class CHL2MP_Localize {
public:
	CHL2MP_Localize() {
		main = new KeyValues( "Translator" );
		//Msg("[CHL2MP_Localize] Class init\n");
	}

	bool AddFile( const char *szFileName ) {
		KeyValues *manifest = new KeyValues( "Translator" );
		if (!manifest->LoadFromFile(filesystem, szFileName))
		{
			manifest->deleteThis();
			return false;
		}

		//Msg("[CHL2MP_Localize] Added file %s\n", szFileName);
		main->RecursiveMergeKeyValues( manifest );
		manifest->deleteThis();
		return true;
	}

	const char *TranslateMsg( const char *szLanguage, char const *msg ) {
		KeyValues *pKV = main->FindKey( msg );
		if( pKV != NULL ) {
			msg = pKV->GetString( szLanguage );
			if( msg[0] == '\0' )
				msg = pKV->GetString( "english", msg );
		}
		
		char *nmsg = strdup( msg );
		Hack_FixEscapeChars( nmsg );
		return nmsg;
	}

	void Clear( void ) {
		//Msg("[CHL2MP_Localize] Cleared cache\n");
		curentlevel = NULL;
		main->Clear();
	}

	char *curentlevel;
	KeyValues *main;
};

static CHL2MP_Localize g_Localize;

const char *UTIL_TranslateMsg( const char *szLanguage, const char *msg ) {
	if( g_Localize.curentlevel != NULL && ( Q_stricmp( g_Localize.curentlevel, STRING( gpGlobals->mapname ) ) != 0 ) )
		g_Localize.Clear();

	if( g_Localize.curentlevel == NULL ) {
		g_Localize.curentlevel = strdup( STRING( gpGlobals->mapname ) );
		//Default translate
		g_Localize.AddFile( "translate.txt" );
		//Custom map translate
		g_Localize.AddFile( UTIL_VarArgs("maps/translate/%s.txt", STRING( gpGlobals->mapname ) ) );
	}
	return g_Localize.TranslateMsg( szLanguage, msg );
}


/*
const char *UTIL_TranslateMsg( const char *szLanguage, const char *msg ) {
	unsigned int i = 0;
	MYSQL *conn = mysql_init(NULL);
	if(conn == NULL)
		fprintf(stderr, "Error: can'tcreate MySQL-descriptor\n");

	if(!mysql_real_connect(conn, "hl2mp.ru", "hl2mp", "wr6yc25u", "hl2mp", NULL, NULL, 0))
		fprintf(stderr, "Error: can'tconnecttodatabase %s\n", mysql_error(conn));
	else {
		mysql_set_character_set(conn, "utf8");
		mysql_query(conn,"SELECT id, text FROM mnu");
		if( MYSQL_RES *res = mysql_store_result( conn ) ) {
			while( MYSQL_ROW row = mysql_fetch_row( res ) ) {
				for( i = 0 ; i < mysql_num_fields( res ); i++ ) {
					printf("String %s\n", row[i]);
				}
			}
		}
		fprintf(stdout, "Success!\n");
	}
	mysql_close(conn);

	sqlite3 *db;
	sqlite3_stmt *stmt;
	
	sqlite3_open("hl2mp/custom/hl2mp.ru/hl2mp.sqlite3", &db);

	if (db == NULL)
	{
		printf("Failed to open DB\n");
		return msg;
	}

	//printf("Performing query...\n");

	const char* SQL = UTIL_VarArgs("SELECT * FROM \"default\" WHERE source = '%s';", msg );
	if( sqlite3_prepare_v2( db, SQL, -1, &stmt, NULL) != 0 )
		return msg;

	//printf("Got results:\n");
	char *nmsg = strdup( msg );

	while (sqlite3_step(stmt) != SQLITE_DONE) {
		bool found = false;
		int i;
		int num_cols = sqlite3_column_count(stmt);
		
		for (i = 0; i < num_cols; i++)
		{
			switch (sqlite3_column_type(stmt, i))
			{
			case (SQLITE3_TEXT):
				if( FStrEq( szLanguage, sqlite3_column_name(stmt, i) ) ) {
					nmsg = strdup( UTIL_VarArgs("%s", sqlite3_column_text(stmt, i) ) );
					found = true;
					break;
				}

				if( FStrEq( sqlite3_column_name(stmt, i), "english" ) )
					nmsg = strdup( UTIL_VarArgs("%s", sqlite3_column_text(stmt, i) ) );

				//Msg("%s %s, ", sqlite3_column_text(stmt, i), sqlite3_column_name(stmt, i) );
				break;
			case (SQLITE_INTEGER):
				printf("%d, ", sqlite3_column_int(stmt, i));
				break;
			case (SQLITE_FLOAT):
				printf("%g, ", sqlite3_column_double(stmt, i));
				break;
			default:
				break;
			}
		}

		if( found )
			break;
	}

	sqlite3_finalize(stmt);

	sqlite3_close(db);
	Hack_FixEscapeChars( nmsg );

	return nmsg;
}*/