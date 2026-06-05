# transit-booking-engine
A clean, high-performance console application that simulates a real-world booking system for airlines and railways. It handles everything from seat layout allocation to real-time ticket pricing adjustments based on demand.

Project Demo :

[Watch the Project Demo Video here] : https://drive.google.com/file/d/12vgtc99SZKbtbNAXVJXrJ5Axjy7lRCMe/view?usp=sharing

[  Why I Built This  ] :

Most basic reservation projects use simple arrays or basic lists to handle bookings. While that works for small assignments, it fails in the real world when hundreds of passengers try to book or cancel tickets simultaneously.
I wanted to build a backend system that handles real-world booking logic efficiently. The goal was to solve three specific problems:

1.) Instant Seat Allocation:    Ensuring the system instantly finds and gives the passenger the best available seat without scanning through the entire vehicle                                     every single time.

2.) Surge Pricing:    Making ticket prices change automatically based on how many seats are left, just like real flight booking apps.

3.) Smart Cancellations:        If someone cancels a ticket, their seat should immediately become available for the next person, or go straight to the person                                      waiting next in line.

[  Core Features ] :

1.) Smart Seat Assignment : 
    Instead of picking seats randomly, the engine uses a priority system to always offer the lowest-numbered free seat first. If a passenger cancels their ticket,     that specific seat goes straight back into the pool and is automatically offered to the very next buyer.

2.) Live Demand Pricing :
    The system features an automatic pricing engine. As more people book tickets on a specific flight or train, the price automatically goes up based on how full      it is:

  Less than 30% full: Normal Base Price
  
  30% to 60% full: 20% price increase
  
  60% to 80% full: 45% price increase
  
  80% to 95% full: 75% price increase
  
  Near full capacity: More than double the base price

3.) Queue-Based Waitlist :
    When a flight or train class is completely full, passengers are not just turned away. They have the option to join a structured waitlist. The system tracks        the exact order in which people joined, ensuring absolute fairness when a seat opens up due to a cancellation.

4.) Interactive Console Layout :
    The system prints out a clean text-based map of the seating arrangement. It shows exactly which seats are free and which ones are occupied, making it easy to      visualize the occupancy of any trip.

[  Tech Stack  ] :

Language :          C++ (Object-Oriented Programming)

Data Management :   C++ Standard Template Library (Vectors, Heaps, and Hash Maps for quick lookups)

[  Key Learnings  ] :

Real-World Logic :          Learned how to translate complex business rules—like live surge pricing and automated waitlist tracking—into clean, reliable object-                               oriented code.

Smart Data Choices :        It showed me how picking the right structure (like Heaps for instant seat allocation and Maps for quick passenger lookups) completely                              changes how fast a system runs.
