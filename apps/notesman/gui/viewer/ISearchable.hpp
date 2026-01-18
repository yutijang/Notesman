#pragma once

class ISearchable {
    public:
        virtual ~ISearchable() = default;

        virtual void startSearch() = 0;  // Ctrl + F
        virtual void findNext() = 0;     // F3
        virtual void findPrevious() = 0; // Shift + F3
};
