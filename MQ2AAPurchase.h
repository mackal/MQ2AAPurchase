#include <string>
#include <vector>

#define SKIPPED_FRAMES 60

struct AAEntry
{
	std::string name;
	DWORD id;				// ID of the AA rank
	DWORD rank;				// Rank you wish to spend to
	DWORD current_rank;		// Current rank we're at
	bool max;				// If true will not check rank and just buy to max

	void clear()
	{
		name.clear();
		id = 0;
		rank = 0;
		current_rank = 0;
		max = false;
	}
};


PALTABILITY GetAAFromName(const char *name);
void LoadINI();
bool HasAa(int id); // there is a client function for this ...
PALTABILITY GetMaxOwned(PALTABILITY aa); // there is a client function that works on group ID for this
VOID cmdAapurchase(PSPAWNINFO pChar, PCHAR szLine);

// Blech callbacks
void __stdcall GainedSomething(unsigned int ID, void *pData, PBLECHVALUE pValues);

DWORD g_banked = 0;
bool g_auton = false;
bool g_autopurchase = false;
int g_position = 0;
bool g_aainit = false;
size_t g_frame_count = 0;
std::vector<AAEntry> aa_list;
Blech *pAAEvents = NULL;
