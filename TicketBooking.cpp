/*
============================================================
   AIRLINE & RAILWAY SEAT RESERVATION + TICKET BOOKING ENGINE
============================================================
*/

#include <iostream>
#include <string>
#include <vector>
#include <map>
#include <unordered_map>
#include <queue>
#include <algorithm>
#include <iomanip>
#include <ctime>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <limits>
#include <climits>

using namespace std;

// ──────────────────────────────────────────────────────────
//   SECTION 1 :  HELPER UTILITIES
// ──────────────────────────────────────────────────────────

// Generate a simple unique PNR like "PNR-1001"
static int g_pnrCounter = 1000;
string generatePNR()
{
    g_pnrCounter++;
    return "PNR-" + to_string(g_pnrCounter);
}

// Get current timestamp as a readable string
string getCurrentTime()
{
    time_t now = time(nullptr);
    char buf[30];
    struct tm* ltime = localtime(&now);
    if (ltime != nullptr) {
        strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M", ltime);
        return string(buf);
    }
    return "0000-00-00 00:00";
}

// Clear the terminal 
void clearScreen()
{
#ifdef _WIN32
    system("cls");
#else
    system("clear");
#endif
}

// Print a separator line
void printLine(char ch = '-', int width = 60)
{
    cout << string(width, ch) << "\n";
}

// Safely read an integer from the user
int readInt(const string& prompt, int lo = INT_MIN, int hi = INT_MAX)
{
    int val;
    while (true)
    {
        cout << prompt;
        if (cin >> val && val >= lo && val <= hi)
        {
            cin.ignore(numeric_limits<streamsize>::max(), '\n');
            return val;
        }
        cin.clear();
        cin.ignore(numeric_limits<streamsize>::max(), '\n');
        cout << "  [!] Invalid input. Please try again.\n";
    }
}

// Safely read a non-empty string
string readString(const string& prompt)
{
    string s;
    cout << prompt;
    getline(cin, s);
    while (s.empty())
    {
        cout << "  [!] Cannot be empty. " << prompt;
        getline(cin, s);
    }
    return s;
}

// ──────────────────────────────────────────────────────────
//   SECTION 2 :   CONSTANTS
// ──────────────────────────────────────────────────────────

enum class SeatClass
{
    ECONOMY   = 0,
    BUSINESS  = 1,
    FIRST     = 2
};

string seatClassName(SeatClass sc)
{
    switch (sc)
    {
        case SeatClass::ECONOMY:  return "Economy";
        case SeatClass::BUSINESS: return "Business";
        case SeatClass::FIRST:    return "First Class";
    }
    return "Unknown";
}

enum class BookingStatus
{
    PENDING,    // seat locked, payment not confirmed
    CONFIRMED,  // payment done
    CANCELLED   // user cancelled or payment failed
};

string statusName(BookingStatus s)
{
    switch (s)
    {
        case BookingStatus::PENDING:   return "PENDING";
        case BookingStatus::CONFIRMED: return "CONFIRMED";
        case BookingStatus::CANCELLED: return "CANCELLED";
    }
    return "UNKNOWN";
}

enum class TripType { FLIGHT, TRAIN };

string tripTypeName(TripType t)
{
    return (t == TripType::FLIGHT) ? "Flight" : "Train";
}

// ──────────────────────────────────────────────────────────
//   SECTION 3 :  SEAT  
// ──────────────────────────────────────────────────────────

struct Seat
{
    int       seatNumber;   // e.g. 1, 2, 3 …
    string    seatLabel;    // e.g. "12A", "14B"
    SeatClass seatClass;
    bool      isBooked;
    string    bookedByPNR;  // which booking holds this seat

    Seat() : seatNumber(0), seatClass(SeatClass::ECONOMY),
             isBooked(false) {}

    Seat(int num, const string& label, SeatClass sc)
        : seatNumber(num), seatLabel(label),
          seatClass(sc), isBooked(false) {}
};

// ──────────────────────────────────────────────────────────
//   SECTION 4 :  PASSENGER
// ──────────────────────────────────────────────────────────

struct Passenger
{
    string passengerId;  // unique id
    string name;
    string email;
    string phone;
    int    age;

    // All PNRs this passenger has ever booked
    vector<string> bookingHistory;

    Passenger() : age(0) {}

    Passenger(const string& id, const string& n,
              const string& e, const string& ph, int a)
        : passengerId(id), name(n), email(e), phone(ph), age(a) {}

    void display() const
    {
        cout << "  Passenger : " << name
             << "  |  ID: " << passengerId
             << "  |  Age: " << age << "\n"
             << "  Email     : " << email
             << "  |  Phone: " << phone << "\n";
    }
};

// ──────────────────────────────────────────────────────────
//   SECTION 5 :  DYNAMIC PRICING ENGINE
//                Price goes up as seats fill up.
// ──────────────────────────────────────────────────────────

class PricingEngine
{
public:
    // basePrice  : price when 0% seats are booked
    // totalSeats : capacity of this class on this trip
    // bookedSeats: how many already booked
    static double calculatePrice(double basePrice,
                                 int totalSeats,
                                 int bookedSeats)
    {
        if (totalSeats <= 0) return basePrice;

        double fillRatio = static_cast<double>(bookedSeats)
                           / static_cast<double>(totalSeats);

        double multiplier = 1.0;

        if      (fillRatio < 0.30) multiplier = 1.00;  // 0–30%  normal
        else if (fillRatio < 0.60) multiplier = 1.20;  // 30–60% +20%
        else if (fillRatio < 0.80) multiplier = 1.45;  // 60–80% +45%
        else if (fillRatio < 0.95) multiplier = 1.75;  // 80–95% +75%
        else                       multiplier = 2.10;  // >95%   +110%

        return basePrice * multiplier;
    }

    // Apply class-level base multiplier
    static double classMultiplier(SeatClass sc)
    {
        switch (sc)
        {
            case SeatClass::ECONOMY:  return 1.0;
            case SeatClass::BUSINESS: return 2.5;
            case SeatClass::FIRST:    return 4.5;
        }
        return 1.0;
    }
};

// ──────────────────────────────────────────────────────────
//   SECTION 6 :  SEAT MAP  (per class, per trip)
// ──────────────────────────────────────────────────────────

class SeatMap
{
private:
    vector<Seat>       seats;
    // Available seat numbers stored in a min-heap so we
    // always offer the lowest-numbered free seat first.
    priority_queue<int,
                   vector<int>,
                   greater<int>> availableQueue;
    int totalCount;
    int bookedCount;

public:
    SeatMap() : totalCount(0), bookedCount(0) {}

    // Build the seat map: count seats, labelled A-F per row
    void initialize(int count, SeatClass sc, int rowOffset = 0)
    {
        totalCount  = count;
        bookedCount = 0;
        seats.clear();

        // Drain old queue
        while (!availableQueue.empty()) availableQueue.pop();

        string colLabels = "ABCDEF";
        int cols = 6; // typical aircraft row width

        for (int i = 0; i < count; i++)
        {
            int row    = (rowOffset + i) / cols + 1;
            char col   = colLabels[(rowOffset + i) % cols];
            string lbl = to_string(row) + col;

            seats.emplace_back(i + 1, lbl, sc);
            availableQueue.push(i + 1);  // seat numbers start at 1
        }
    }

    int total()  const { return totalCount;  }
    int booked() const { return bookedCount; }
    int free()   const { return totalCount - bookedCount; }

    // Returns the seat number of the next best free seat, -1 if none
    int nextFreeSeat()
    {
        // Clean stale entries (seats that were un-booked and re-added)
        while (!availableQueue.empty())
        {
            int top = availableQueue.top();
            int idx = top - 1;
            if (idx >= 0 && idx < (int)seats.size()
                && !seats[idx].isBooked)
                return top;
            availableQueue.pop();  // stale, remove
        }
        return -1;
    }

    // Lock a seat for a PNR (called during booking, before payment)
    bool lockSeat(int seatNum, const string& pnr)
    {
        int idx = seatNum - 1;
        if (idx < 0 || idx >= (int)seats.size()) return false;
        if (seats[idx].isBooked)                 return false;

        seats[idx].isBooked    = true;
        seats[idx].bookedByPNR = pnr;
        bookedCount++;

        // Remove from priority queue (lazy deletion: will be cleaned on next nextFreeSeat())
        return true;
    }

    // Release a seat back (called on cancel or payment failure)
    bool releaseSeat(int seatNum)
    {
        int idx = seatNum - 1;
        if (idx < 0 || idx >= (int)seats.size()) return false;
        if (!seats[idx].isBooked)                return false;

        seats[idx].isBooked    = false;
        seats[idx].bookedByPNR = "";
        bookedCount--;

        availableQueue.push(seatNum);  // back in the queue
        return true;
    }

    // Pretty-print the seat map
    void display() const
    {
        cout << "  Seat Map  (O = free,  X = booked)\n  ";
        string colLabels = "ABCDEF";
        int cols = 6;

        for (size_t c = 0; c < colLabels.size(); c++) cout << " " << colLabels[c];
        cout << "\n";

        for (int i = 0; i < (int)seats.size(); i++)
        {
            if (i % cols == 0)
            {
                int row = i / cols + 1;
                cout << "  " << setw(2) << row;
            }
            cout << " " << (seats[i].isBooked ? "X" : "O");
            if ((i + 1) % cols == 0) cout << "\n";
        }
        if (seats.size() % cols != 0) cout << "\n";
    }

    // Get label for a seat number
    string getSeatLabel(int seatNum) const
    {
        int idx = seatNum - 1;
        if (idx < 0 || idx >= (int)seats.size()) return "??";
        return seats[idx].seatLabel;
    }
};

// ──────────────────────────────────────────────────────────
//   SECTION 7 :  BOOKING  (one ticket = one booking)
// ──────────────────────────────────────────────────────────

struct Booking
{
    string         pnr;
    string         tripId;
    string         passengerId;
    SeatClass      seatClass;
    int            seatNumber;
    string         seatLabel;
    double         pricePaid;
    BookingStatus  status;
    string         bookedAt;
    string         cancelledAt;

    Booking() : seatNumber(0), pricePaid(0.0),
                status(BookingStatus::PENDING) {}

    void display() const
    {
        printLine();
        cout << "  PNR            : " << pnr          << "\n"
             << "  Trip ID        : " << tripId        << "\n"
             << "  Passenger ID  : " << passengerId   << "\n"
             << "  Class          : " << seatClassName(seatClass) << "\n"
             << "  Seat           : " << seatLabel
             << " (#" << seatNumber << ")\n"
             << "  Price Paid    : INR " << fixed
             << setprecision(2) << pricePaid          << "\n"
             << "  Status         : " << statusName(status) << "\n"
             << "  Booked At     : " << bookedAt       << "\n";
        if (status == BookingStatus::CANCELLED)
            cout << "  Cancelled At  : " << cancelledAt  << "\n";
        printLine();
    }
};

// ──────────────────────────────────────────────────────────
//   SECTION 8 :  TRIP  (one flight or train journey)
// ──────────────────────────────────────────────────────────

class Trip
{
private:
    // seat maps indexed by SeatClass
    map<SeatClass, SeatMap>    seatMaps;
    map<SeatClass, double>      basePrices;   // base price per class

    // Waitlist per class: (priority, passengerId)
    map<SeatClass,
        priority_queue<pair<int,string>,
                       vector<pair<int,string>>,
                       greater<pair<int,string>>>> waitlist;

    static int g_waitPriority;  // monotonically increasing to preserve FIFO

public:
    string   tripId;
    TripType type;
    string   origin;
    string   destination;
    string   departureTime;
    string   arrivalTime;

    Trip() {}

    Trip(const string& id, TripType t,
         const string& orig, const string& dest,
         const string& dep,  const string& arr)
        : tripId(id), type(t), origin(orig),
          destination(dest), departureTime(dep), arrivalTime(arr) {}

    // Add a seat class with capacity and base price
    void addClass(SeatClass sc, int seats, double price)
    {
        seatMaps[sc].initialize(seats, sc);
        basePrices[sc] = price;
    }

    // Current price for a class (dynamic)
    double currentPrice(SeatClass sc)
    {
        if (seatMaps.find(sc) == seatMaps.end()) return 0.0;
        SeatMap& sm = seatMaps[sc];
        double base = basePrices[sc]
                    * PricingEngine::classMultiplier(sc);
        return PricingEngine::calculatePrice(
                   base, sm.total(), sm.booked());
    }

    // Try to lock the best available seat for a class
    int lockBestSeat(SeatClass sc, const string& pnr)
    {
        if (seatMaps.find(sc) == seatMaps.end()) return -1;
        SeatMap& sm = seatMaps[sc];
        int seatNum = sm.nextFreeSeat();
        if (seatNum < 0) return -1;
        sm.lockSeat(seatNum, pnr);
        return seatNum;
    }

    // Release a seat (on cancel / payment fail)
    void releaseSeat(SeatClass sc, int seatNum)
    {
        if (seatMaps.count(sc))
            seatMaps[sc].releaseSeat(seatNum);
    }

    // Get seat label
    string getSeatLabel(SeatClass sc, int num)
    {
        if (!seatMaps.count(sc)) return "??";
        return seatMaps[sc].getSeatLabel(num);
    }

    // Add to waitlist
    void addToWaitlist(SeatClass sc, const string& passId)
    {
        g_waitPriority++;
        waitlist[sc].push({g_waitPriority, passId});
        cout << "  [+] Added to waitlist. Position: "
             << waitlist[sc].size() << "\n";
    }

    // Pop from waitlist
    string popWaitlist(SeatClass sc)
    {
        if (waitlist[sc].empty()) return "";
        string pid = waitlist[sc].top().second;
        waitlist[sc].pop();
        return pid;
    }

    bool hasWaiting(SeatClass sc)
    {
        return !waitlist[sc].empty();
    }

    // --- Display helpers ---

    void displaySummary() const
    {
        cout << "  ID      : " << tripId
             << "  [" << tripTypeName(type) << "]\n"
             << "  Route   : " << origin << "  -->  " << destination << "\n"
             << "  Depart  : " << departureTime
             << "     Arrive: " << arrivalTime << "\n";

        for (auto it = seatMaps.begin(); it != seatMaps.end(); ++it)
        {
            SeatClass sc = it->first;
            const SeatMap& sm = it->second;
            cout << "  Class   : " << seatClassName(sc)
                 << "  |  Seats: " << sm.free() << "/" << sm.total()
                 << "  |  Price: INR "
                 << fixed << setprecision(2)
                 << const_cast<Trip*>(this)->currentPrice(sc)
                 << "\n";
        }
    }

    void displaySeatMap(SeatClass sc)
    {
        if (!seatMaps.count(sc))
        {
            cout << "  This class is not available on this trip.\n";
            return;
        }
        cout << "\n  === " << seatClassName(sc)
             << " Seat Map ===\n";
        seatMaps[sc].display();
        cout << "  Free: " << seatMaps[sc].free()
             << "  /  Total: " << seatMaps[sc].total() << "\n\n";
    }

    // Check if a class exists and has free seats
    bool hasSeats(SeatClass sc)
    {
        return seatMaps.count(sc) && seatMaps[sc].free() > 0;
    }

    // All available classes on this trip
    vector<SeatClass> availableClasses()
    {
        vector<SeatClass> result;
        for (auto it = seatMaps.begin(); it != seatMaps.end(); ++it)
        {
            if (it->second.total() > 0) result.push_back(it->first);
        }
        return result;
    }

    int totalRevenue(map<string, Booking>& allBookings)
    {
        double rev = 0;
        for (auto it = allBookings.begin(); it != allBookings.end(); ++it)
        {
            const Booking& b = it->second;
            if (b.tripId == tripId && b.status == BookingStatus::CONFIRMED)
                rev += b.pricePaid;
        }
        return static_cast<int>(rev);
    }
};

int Trip::g_waitPriority = 0;

// ──────────────────────────────────────────────────────────
//   SECTION 9 :  CENTRAL BOOKING SYSTEM
// ──────────────────────────────────────────────────────────

class BookingSystem
{
private:
    unordered_map<string, Trip>      trips;       // tripId  -> Trip
    unordered_map<string, Booking>    bookings;    // pnr     -> Booking
    unordered_map<string, Passenger> passengers;  // passId  -> Passenger

    static int g_passengerCounter;

    // Find or create a passenger
    string getOrCreatePassenger(const string& name,
                                const string& email,
                                const string& phone,
                                int age)
    {
        for (auto it = passengers.begin(); it != passengers.end(); ++it)
        {
            if (it->second.email == email)
                return it->first;
        }

        g_passengerCounter++;
        string pid = "PASS-" + to_string(g_passengerCounter);
        passengers[pid] = Passenger(pid, name, email, phone, age);
        cout << "  [+] New passenger profile created: " << pid << "\n";
        return pid;
    }

public:

    // ── Add a trip ────────────────────────────────────────
    void addTrip(const string& id, TripType type,
                 const string& orig, const string& dest,
                 const string& dep,  const string& arr)
    {
        if (trips.count(id))
        {
            cout << "  [!] Trip ID " << id << " already exists.\n";
            return;
        }
        trips.emplace(id, Trip(id, type, orig, dest, dep, arr));
        cout << "  [+] Trip " << id << " added.\n";
    }

    // ── Add a class to a trip ─────────────────────────────
    void addClassToTrip(const string& tripId,
                        SeatClass sc, int seats, double price)
    {
        if (!trips.count(tripId))
        {
            cout << "  [!] Trip not found.\n";
            return;
        }
        trips.at(tripId).addClass(sc, seats, price);
        cout << "  [+] " << seatClassName(sc)
             << " class added to " << tripId
             << " (" << seats << " seats @ INR "
             << price << " base)\n";
    }

    // ── List all trips ────────────────────────────────────
    void listTrips()
    {
        if (trips.empty())
        {
            cout << "  No trips available.\n";
            return;
        }
        int idx = 1;
        for (auto it = trips.begin(); it != trips.end(); ++it)
        {
            cout << "\n  [" << idx++ << "] ";
            it->second.displaySummary();
            printLine();
        }
    }

    // ── BOOK a ticket (multi-step) ────────────────────────
    void bookTicket()
    {
        printLine('=');
        cout << "         BOOK A TICKET\n";
        printLine('=');

        if (trips.empty())
        {
            cout << "  No trips available right now.\n";
            return;
        }

        listTrips();

        string tripId = readString("  Enter Trip ID to book: ");
        if (!trips.count(tripId))
        {
            cout << "  [!] Trip not found.\n";
            return;
        }

        Trip& trip = trips.at(tripId);

        cout << "\n  Available Classes:\n";
        auto classes = trip.availableClasses();
        for (int i = 0; i < (int)classes.size(); i++)
        {
            cout << "  [" << (i + 1) << "] "
                 << seatClassName(classes[i])
                 << "  –  INR "
                 << fixed << setprecision(2)
                 << trip.currentPrice(classes[i])
                 << " (";
            if (!trip.hasSeats(classes[i]))
                cout << "FULL – waitlist available";
            else
                cout << "seats available";
            cout << ")\n";
        }

        int classChoice = readInt("  Choose class [1-"
                           + to_string(classes.size()) + "]: ",
                           1, (int)classes.size());
        SeatClass chosenClass = classes[classChoice - 1];

        trip.displaySeatMap(chosenClass);

        cout << "\n  --- Passenger Details ---\n";
        string name  = readString("  Full Name   : ");
        string email = readString("  Email       : ");
        string phone = readString("  Phone       : ");
        int    age   = readInt   ("  Age         : ", 1, 120);

        string passId = getOrCreatePassenger(name, email, phone, age);

        if (!trip.hasSeats(chosenClass))
        {
            cout << "\n  [!] No seats available in "
                 << seatClassName(chosenClass) << ".\n";
            int wl = readInt("  Join waitlist? (1=Yes / 0=No): ", 0, 1);
            if (wl == 1)
                trip.addToWaitlist(chosenClass, passId);
            return;
        }

        string pnr    = generatePNR();
        int seatNum   = trip.lockBestSeat(chosenClass, pnr);
        string seatLbl = trip.getSeatLabel(chosenClass, seatNum);
        double price  = trip.currentPrice(chosenClass);

        Booking bk;
        bk.pnr         = pnr;
        bk.tripId      = tripId;
        bk.passengerId = passId;
        bk.seatClass   = chosenClass;
        bk.seatNumber  = seatNum;
        bk.seatLabel   = seatLbl;
        bk.pricePaid   = price;
        bk.status      = BookingStatus::PENDING;
        bk.bookedAt    = getCurrentTime();

        bookings[pnr] = bk;
        passengers[passId].bookingHistory.push_back(pnr);

        printLine('=');
        cout << "\n  BOOKING SUMMARY (PENDING PAYMENT)\n\n"
             << "  PNR          : " << pnr      << "\n"
             << "  Trip         : " << tripId   << "\n"
             << "  Route        : " << trip.origin
             << " --> " << trip.destination << "\n"
             << "  Depart       : " << trip.departureTime << "\n"
             << "  Passenger    : " << name     << "\n"
             << "  Seat         : " << seatLbl
             << " (" << seatClassName(chosenClass) << ")\n"
             << "\n  Amount Due   : INR "
             << fixed << setprecision(2) << price << "\n\n";
        printLine('=');

        cout << "  [Payment Gateway Simulation]\n"
             << "  1. Confirm Payment (pay INR "
             << fixed << setprecision(2) << price << ")\n"
             << "  2. Cancel Booking\n";
        int payChoice = readInt("  Your choice [1/2]: ", 1, 2);

        if (payChoice == 1)
        {
            bookings[pnr].status = BookingStatus::CONFIRMED;
            cout << "\n  ✓  Payment successful!\n"
                 << "  ✓  Booking CONFIRMED.\n"
                 << "  ✓  Your PNR is : " << pnr << "\n"
                 << "  ✓  Seat        : " << seatLbl << "\n";
        }
        else
        {
            bookings[pnr].status      = BookingStatus::CANCELLED;
            bookings[pnr].cancelledAt = getCurrentTime();
            trip.releaseSeat(chosenClass, seatNum);

            if (trip.hasWaiting(chosenClass))
            {
                string waitPassId = trip.popWaitlist(chosenClass);
                if (passengers.count(waitPassId))
                {
                    cout << "\n  [Waitlist] Notifying "
                         << passengers[waitPassId].name
                         << " that a seat is now available!\n";
                }
            }

            cout << "\n  [!] Booking cancelled. Seat released.\n"
                 << "  [!] PNR " << pnr
                 << " is now CANCELLED.\n";
        }
    }

    // ── Cancel an existing CONFIRMED booking ─────────────
    void cancelBooking()
    {
        printLine('=');
        cout << "         CANCEL BOOKING\n";
        printLine('=');

        string pnr = readString("  Enter PNR to cancel: ");
        if (!bookings.count(pnr))
        {
            cout << "  [!] PNR not found.\n";
            return;
        }

        Booking& bk = bookings[pnr];

        if (bk.status == BookingStatus::CANCELLED)
        {
            cout << "  [!] This booking is already cancelled.\n";
            return;
        }
        if (bk.status == BookingStatus::PENDING)
        {
            cout << "  [!] Payment not completed. Use the booking flow.\n";
            return;
        }

        bk.display();
        int confirm = readInt("  Confirm cancellation? (1=Yes / 0=No): ", 0, 1);
        if (confirm == 0) { cout << "  Cancellation aborted.\n"; return; }

        bk.status      = BookingStatus::CANCELLED;
        bk.cancelledAt = getCurrentTime();

        if (trips.count(bk.tripId))
            trips.at(bk.tripId).releaseSeat(bk.seatClass, bk.seatNumber);

        cout << "\n  ✓  Booking " << pnr << " cancelled.\n"
             << "  ✓  Seat " << bk.seatLabel << " is now free.\n";
    }

    // ── PNR Lookup ────────────────────────────────────────
    void lookupPNR()
    {
        string pnr = readString("  Enter PNR: ");
        if (!bookings.count(pnr))
        {
            cout << "  [!] PNR not found.\n";
            return;
        }
        bookings[pnr].display();

        string pid = bookings[pnr].passengerId;
        if (passengers.count(pid))
            passengers[pid].display();
    }

    // ── Passenger history ─────────────────────────────────
    void passengerHistory()
    {
        string email = readString("  Enter your email: ");
        string pid;
        for (auto it = passengers.begin(); it != passengers.end(); ++it)
        {
            if (it->second.email == email) { pid = it->first; break; }
        }
        if (pid.empty())
        {
            cout << "  [!] No passenger found with that email.\n";
            return;
        }

        Passenger& p = passengers[pid];
        p.display();
        cout << "\n  Booking History:\n";
        if (p.bookingHistory.empty())
        {
            cout << "  (no bookings yet)\n";
            return;
        }
        for (size_t i = 0; i < p.bookingHistory.size(); i++)
        {
            string b = p.bookingHistory[i];
            if (bookings.count(b))
                bookings[b].display();
        }
    }

    // ── Admin: View all bookings for a trip ───────────────
    void adminTripManifest()
    {
        string tripId = readString("  Enter Trip ID: ");
        if (!trips.count(tripId))
        {
            cout << "  [!] Trip not found.\n";
            return;
        }

        printLine('=');
        cout << "  MANIFEST for " << tripId << "\n";
        printLine('=');
        trips.at(tripId).displaySummary();
        printLine();

        int count = 0;
        double revenue = 0;
        for (auto it = bookings.begin(); it != bookings.end(); ++it)
        {
            const Booking& bk = it->second;
            if (bk.tripId != tripId || bk.status == BookingStatus::CANCELLED)
                continue;
            count++;
            revenue += bk.pricePaid;
            cout << "  " << setw(12) << left << bk.pnr
                 << "  Seat: " << setw(5) << bk.seatLabel
                 << "  Class: " << setw(12)
                 << seatClassName(bk.seatClass)
                 << "  INR " << fixed << setprecision(2)
                 << bk.pricePaid
                 << "  " << statusName(bk.status) << "\n";
        }
        printLine();
        cout << "  Total Passengers : " << count << "\n"
             << "  Total Revenue    : INR "
             << fixed << setprecision(2) << revenue << "\n";
        printLine('=');
    }

    // ── Admin: View seat map for a trip+class ─────────────
    void adminSeatMap()
    {
        string tripId = readString("  Enter Trip ID: ");
        if (!trips.count(tripId))
        {
            cout << "  [!] Trip not found.\n";
            return;
        }
        cout << "  Classes: 0=Economy  1=Business  2=FirstClass\n";
        int c = readInt("  Enter class number: ", 0, 2);
        SeatClass sc = static_cast<SeatClass>(c);
        trips.at(tripId).displaySeatMap(sc);
    }

    // ── Pre-load demo trips ────────────────────────────────
    void loadDemoData()
    {
        addTrip("AI-101", TripType::FLIGHT,
                "Delhi (DEL)", "Mumbai (BOM)",
                "2025-12-01 06:00", "2025-12-01 08:10");
        addClassToTrip("AI-101", SeatClass::ECONOMY,  120, 4500.0);
        addClassToTrip("AI-101", SeatClass::BUSINESS,  24, 4500.0);
        addClassToTrip("AI-101", SeatClass::FIRST,      6, 4500.0);

        addTrip("6E-212", TripType::FLIGHT,
                "Bangalore (BLR)", "Chennai (MAA)",
                "2025-12-02 09:30", "2025-12-02 10:45");
        addClassToTrip("6E-212", SeatClass::ECONOMY, 180, 2800.0);
        addClassToTrip("6E-212", SeatClass::BUSINESS,  18, 2800.0);

        addTrip("SG-445", TripType::FLIGHT,
                "Kolkata (CCU)", "Hyderabad (HYD)",
                "2025-12-03 14:00", "2025-12-03 16:30");
        addClassToTrip("SG-445", SeatClass::ECONOMY, 150, 3200.0);
        addClassToTrip("SG-445", SeatClass::BUSINESS,  30, 3200.0);

        addTrip("RAJ-01", TripType::TRAIN,
                "New Delhi", "Agra",
                "2025-12-01 06:00", "2025-12-01 08:00");
        addClassToTrip("RAJ-01", SeatClass::ECONOMY,   72, 800.0);
        addClassToTrip("RAJ-01", SeatClass::BUSINESS,  48, 800.0);
        addClassToTrip("RAJ-01", SeatClass::FIRST,     12, 800.0);

        addTrip("DUR-22", TripType::TRAIN,
                "Mumbai CST", "Pune",
                "2025-12-02 07:15", "2025-12-02 10:30");
        addClassToTrip("DUR-22", SeatClass::ECONOMY, 200, 350.0);
        addClassToTrip("DUR-22", SeatClass::BUSINESS,  60, 350.0);

        cout << "\n  [-] Demo data loaded (3 flights + 2 trains).\n\n";
    }
};

int BookingSystem::g_passengerCounter = 100;

// ──────────────────────────────────────────────────────────
//   SECTION 10 :  MENUS
// ──────────────────────────────────────────────────────────

void showMainMenu()
{
    printLine('=');
    cout << "   ✈  TICKET BOOKING ENGINE  🚂\n";
    printLine('=');
    cout << "  1. View all Trips\n"
         << "  2. Book a Ticket\n"
         << "  3. Cancel a Booking\n"
         << "  4. Check PNR Status\n"
         << "  5. My Booking History\n"
         << "  6. Admin Panel\n"
         << "  0. Exit\n";
    printLine('=');
}

void showAdminMenu()
{
    printLine('=');
    cout << "         ADMIN PANEL\n";
    printLine('=');
    cout << "  1. View Trip Manifest & Revenue\n"
         << "  2. View Seat Map\n"
         << "  3. Add New Trip\n"
         << "  4. Add Class to Trip\n"
         << "  0. Back to Main Menu\n";
    printLine('=');
}

// ──────────────────────────────────────────────────────────
//   SECTION 11 :  MAIN
// ──────────────────────────────────────────────────────────

int main()
{
    BookingSystem system;

    cout << "\n  Initializing system …\n";
    system.loadDemoData();

    bool running = true;
    while (running)
    {
        showMainMenu();
        int choice = readInt("  Your choice: ", 0, 6);

        switch (choice)
        {
        case 1:
            printLine('=');
            cout << "  ALL AVAILABLE TRIPS\n";
            printLine('=');
            system.listTrips();
            break;

        case 2:
            system.bookTicket();
            break;

        case 3:
            system.cancelBooking();
            break;

        case 4:
            system.lookupPNR();
            break;

        case 5:
            system.passengerHistory();
            break;

        case 6:
        {
            bool adminRunning = true;
            while (adminRunning)
            {
                showAdminMenu();
                int ac = readInt("  Admin choice: ", 0, 4);
                switch (ac)
                {
                case 1: system.adminTripManifest(); break;
                case 2: system.adminSeatMap();      break;
                case 3:
                {
                    string id   = readString("  Trip ID     : ");
                    cout << "  Type (0=Flight / 1=Train): ";
                    int tt = readInt("", 0, 1);
                    TripType type = (tt == 0) ? TripType::FLIGHT
                                              : TripType::TRAIN;
                    string orig = readString("  Origin      : ");
                    string dest = readString("  Destination : ");
                    string dep  = readString("  Depart Time : ");
                    string arr  = readString("  Arrive Time : ");
                    system.addTrip(id, type, orig, dest, dep, arr);
                    break;
                }
                case 4:
                {
                    string id = readString("  Trip ID: ");
                    cout << "  Class (0=Economy / 1=Business / 2=First): ";
                    int c   = readInt("", 0, 2);
                    int cnt = readInt("  Number of seats  : ", 1, 1000);
                    int pr  = readInt("  Base price (INR) : ", 1, 999999);
                    system.addClassToTrip(
                        id, static_cast<SeatClass>(c), cnt, (double)pr);
                    break;
                }
                case 0: adminRunning = false; break;
                }
            }
            break;
        }

        case 0:
            cout << "\n  Thank you for using the Booking Engine. Bye!\n\n";
            running = false;
            break;
        }
    }

    return 0;
}
