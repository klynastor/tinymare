// Switch rooms for an object without triggering anything.
// Usually used during database loading/conversion.
//
void switch_room(dbref player, dbref loc)
{
  dbref old=db[player].location;

  /* Check object validity */
  if(Typeof(player) == TYPE_ROOM || Typeof(player) == TYPE_ZONE)
    return;

  /* Home defaults to the player's @link */
  if(loc == HOME)
    loc=db[player].link;

  /* Remove object from old room's contents list */
  if(Typeof(old) == TYPE_EXIT)
    db[old].exits = remove_first(db[old].exits, player);
  else
    db[old].contents = remove_first(db[old].contents, player);

  /* Move object */
  db[player].location=loc;

  /* Enter object on top of contents/exits stack */
  if(Typeof(loc) == TYPE_EXIT)
    ADDLIST(player, db[loc].exits);
  else
    ADDLIST(player, db[loc].contents);
}
