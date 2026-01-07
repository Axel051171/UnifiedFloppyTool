#ifndef UFT_CW_H
#define UFT_CW_H

class Fluxmap;
class Bytes;

extern std::unique_ptr<Fluxmap> decodeCatweaselData(
    const Bytes& bytes, nanoseconds_t clock);

#endif
