Players Joining
---------------

This is an important phase in game setup. In the ID creation document we discussed about how cashiers and dealers are registered by the RA(Registration Authority). Here we see more details about the tables and how player finds and joins the tables. 

### Table

The initial thought was dealers create the tables whenever the dealer want to host the table. Some drawbacks we saw with that approach are:
1. The control should be given to the dealer to create the ID's, and we definetely don't any spam of ID's incase if the intentional behavior of any entity changed to malicious.
2. If a new table ID gets created for game that is played then it won't be possible to use catchy names for the tables. The fixed table names provides lots flexibility either in hosting them for the dealers and for the players in configuring them.
3. With new table ID's for every game its difficult host the private tables.

So by observing the limitations, first we taken away(not given) the ability to create the ID's to the dealers, instead dealer can make a request to the cashiers about the creation of table ID's at the time dealer registration or later. Dealer can make five such requests to the cashiers, meaning dealer can host five tables at any given time. Ofcouse `5` is not the hard cap we setting on the number of tables and it can be subject to revision based on the feedback we receive. Likewise we will also allow dealer to make a request to revoke and revocer the table ID.

The good thing with the fixed table names is that, players can configure them in their `verus_player.ini` configuration file and can be choosy about the table the player wants to join. 

### How players find the table

With that intro about tables, here are the steps that player follows to finds out and joins the table:

1. Players get to know about the list of avaiable from dealers ID `dealers.poker.chips10sec@`.
2. After getting the dealers ID's, players fetch the information about the tables that a specific is hosting from the ID `<dealer_name>.poker.chips10sec@`.
3. After going through each table information, player comes to know about the table details like min stake, big blind, empty or not, etc... If player finds the table suitable then player deposit the funds needed to join the table to the `cashiers.poker.chips10sec@` and also in data part of the transaction the player mentions the following details:
```
{
  table_id:"some_table_id";
  primaryaddress: "The address which is owned by the player"; #This address can alse be configured in verus_player.ini config file.
}
```
4. Cashier nodes periodically checks if any deposits are made to the address `cashiers.poker.chips10sec@` using `blocknotify`. The moment cashiers detect any deposits made to the cashiers address, they immediately parse the data part of the tx, and add players `primaryaddress` mentioned in the data part of tx to the `table_id` which is also mentioned by the player in the same data part. Once after cashier adds the players primaryaddress to the `primaryaddresses` of the table_id, from that moment the player can be able to update corresponding `table_id`.

### What goes into table
...