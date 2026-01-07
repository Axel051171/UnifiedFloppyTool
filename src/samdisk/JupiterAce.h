#ifndef UFT_UFT_SAM_JUPITERACE_H
#define UFT_UFT_SAM_JUPITERACE_H

// Jupiter Ace helper functions

int GetDeepThoughtDataOffset(const Data& data);
std::string GetDeepThoughtData(const Data& data);
bool IsDeepThoughtSector(const Sector& sector, int& data_offset);
bool IsDeepThoughtDisk(Disk& disk, const Sector*& sector);
bool IsValidDeepThoughtData(const Data& data);

#endif /* UFT_UFT_SAM_JUPITERACE_H */
