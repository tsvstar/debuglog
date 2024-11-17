#pragma once

namespace tsv::debuglog::tests
{

// Sample test class
struct KnownClass
{
     int x;
     int y;
     bool operator<(const KnownClass& rhs) const
     {
          return (x<rhs.x || (x==rhs.x && y<rhs.y));
     }
     bool operator==(const KnownClass& rhs) const
     {
          return (x==rhs.x && y==rhs.y);
     }
};

}
