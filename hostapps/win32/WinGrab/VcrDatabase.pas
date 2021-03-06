////////////////////////////////////////////////////////////////////////////////
//
// $Log: VcrDatabase.pas,v $
// Revision 1.5  2005/05/25 11:23:44  thotto
// *** empty log message ***
//
// Revision 1.4  2004/12/03 16:09:49  thotto
// - Bugfixes
// - EPG suchen �berarbeitet
// - Timerverwaltung ge�ndert
// - Ruhezustand / Standby gefixt
//
// Revision 1.3  2004/10/15 13:39:16  thotto
// neue Db-Klasse
//
// Revision 1.2  2004/10/11 15:33:39  thotto
// Bugfixes
//
// Revision 1.1  2004/07/02 14:02:37  thotto
// initial
//
//

////////////////////////////////////////////////////////////////////////////////
//
//  DataBase - Procedures ...
//
procedure TfrmMain.OpenEpgDb;
begin
  m_Trace.DBMSG(TRACE_CALLSTACK, '> OpenEpgDb');
  try
    try
      Recorded_ADOConnection.ConnectionString := m_Recorded_dbConnectionString;
      Recorded_ADOConnection.Open;
    except
      on E: Exception do m_Trace.DBMSG(TRACE_ERROR,'OpenEpgDb ( Connect ) '+ E.Message );
    end;
    try
      Whishes_ADOConnection.ConnectionString := m_Recorded_dbConnectionString;
      Whishes_ADOConnection.Open;
    except
      on E: Exception do m_Trace.DBMSG(TRACE_ERROR,'OpenEpgDb ( Connect ) '+ E.Message );
    end;
    try
      Work_ADOConnection.ConnectionString := m_Recorded_dbConnectionString;
      Work_ADOConnection.Open;
    except
      on E: Exception do m_Trace.DBMSG(TRACE_ERROR,'OpenEpgDb ( Connect ) '+ E.Message );
    end;
    try
      Planner_ADOConnection.ConnectionString := m_Recorded_dbConnectionString;
      Planner_ADOConnection.Open;
      Planner_ADOTable.Active:= true;
    except
      on E: Exception do m_Trace.DBMSG(TRACE_ERROR,'OpenEpgDb ( Connect ) '+ E.Message );
    end;

    try
      Recorded_ADOQuery.Close;
      Recorded_ADOQuery.SQL.Clear;
      Recorded_ADOQuery.SQL.Add('SELECT * FROM v_Recorded ORDER BY Titel;');
      Recorded_ADOQuery.ExecSQL;
      Recorded_ADOQuery.Active := true;
    except
      on E: Exception do m_Trace.DBMSG(TRACE_ERROR,'OpenEpgDb / open v_Recorded  '+ E.Message );
    end;
    try
      Whishes_ADOQuery.Close;
      Whishes_ADOQuery.SQL.Clear;
      Whishes_ADOQuery.SQL.Add('SELECT * FROM T_Whishes ORDER BY Titel;');
      Whishes_ADOQuery.ExecSQL;
      Whishes_ADOQuery.Active := true;
    except
      on E: Exception do m_Trace.DBMSG(TRACE_ERROR,'OpenEpgDb / open T_Whishes  '+ E.Message );
    end;
    try
      Work_ADOQuery.Close;
      Work_ADOQuery.SQL.Clear;
    except
      on E: Exception do m_Trace.DBMSG(TRACE_ERROR,'OpenEpgDb / open T_Whishes  '+ E.Message );
    end;

    CheckDbVersion;
  except
    on E: Exception do m_Trace.DBMSG(TRACE_ERROR, 'OpenEpgDb '+ E.Message );
  end;
  m_Trace.DBMSG(TRACE_CALLSTACK, '< OpenEpgDb');
end;

procedure TfrmMain.CloseEpgDb();
begin
  m_Trace.DBMSG(TRACE_CALLSTACK, '> CloseEpgDb');
  try
    Recorded_ADOConnection.Close;
    Whishes_ADOConnection.Close;
    Work_ADOConnection.Close;
  except
    on E: Exception do m_Trace.DBMSG(TRACE_ERROR, 'OpenEpgDb '+ E.Message );
  end;
  m_Trace.DBMSG(TRACE_CALLSTACK, '< CloseEpgDb');
end;

////////////////////////////////////////////////////////////////////////////////

procedure TfrmMain.CheckDbVersion;
var
  sTmp,
  sVersion,
  sSQL : String;
begin
  m_Trace.DBMSG(TRACE_CALLSTACK, '> CheckDbVersion');
  try
    Work_ADOQuery.SQL.Clear;
    sSQL :=        'SELECT * FROM ';
    sSQL := sSQL + 'T_DbVersion ;';
    Work_ADOQuery.SQL.Add(sSQL);
    try
      Work_ADOQuery.Active := true;
      if not Work_ADOQuery.Recordset.Eof then
      begin
        sVersion := VarToStrDef(Work_ADOQuery.Recordset.Fields['Version'].Value, '0');
        if sVersion = '0' then
        begin
{
          sSQL :=        'UPDATE T_Events ';
          sSQL := sSQL + 'SET Zeit = '''     + CheckDbString(sZeit) + ''', ';
          sSQL := sSQL + 'SET Dauer = '''    + CheckDbString(sDauer) + ''', ';
          sSQL := sSQL + 'SET Titel = '''    + CheckDbString(sTitel) + ''', ';
          sSQL := sSQL + 'SET SubTitel = ''' + CheckDbString(sSubTitel) + ''', ';
          sSQL := sSQL + 'SET Epg = '''      + CheckDbString(sEpg) + ''' ';
          sSQL := sSQL + 'WHERE ';
          sSQL := sSQL + 'ChannelId = ''' + IntToStr(ChannelId) + ''' AND ';
          sSQL := sSQL + 'EventId   = ''' + CheckDbString(sEventId) + ''';';
          Work_ADOQuery.SQL.Clear;
          Work_ADOQuery.SQL.Add(sSQL);
          Work_ADOQuery.ExecSQL;
          Work_ADOQuery.Close;
 }
          Work_ADOQuery.SQL.Clear;
        end;
      end;
    except
      on E: Exception do
      begin
        Work_ADOQuery.SQL.Clear;
        sSQL :=        'CREATE TABLE ';
        sSQL := sSQL + 'T_DbVersion ( ';
        sSQL := sSQL + 'Version     NUMBER(10), ';
        sSQL := sSQL + 'Description VARCHAR2(255) ';
        sSQL := sSQL + ')';
        Work_ADOQuery.SQL.Add(sSQL);
        try
          Work_ADOQuery.ExecSQL;
          Work_ADOQuery.Close;
        except
        end;
        Work_ADOQuery.SQL.Clear;
        sSQL :=        'INSERT INTO ';
        sSQL := sSQL + 'T_DbVersion ( ';
        sSQL := sSQL + 'Version, Description ) VALUES ( 1, ''''Initial'''' ';
        sSQL := sSQL + ')';
        Work_ADOQuery.SQL.Add(sSQL);
        try
          Work_ADOQuery.ExecSQL;
          Work_ADOQuery.Close;
        except
        end;
        Work_ADOQuery.SQL.Clear;
      end;
    end;

  except
    on E: Exception do m_Trace.DBMSG(TRACE_ERROR, 'OpenEpgDb '+ E.Message );
  end;
  m_Trace.DBMSG(TRACE_CALLSTACK, '< CheckDbVersion = '+sVersion);
end;

////////////////////////////////////////////////////////////////////////////////

procedure TfrmMain.Recorded_DBGridTitleClick(Column: TColumn);
var
  sSQL: String;
begin
  try
    Recorded_ADOQuery.SQL.Clear;
    sSQL :=        'SELECT * FROM ';
    sSQL := sSQL + 'v_Recorded ';
    sSQL := sSQL + ' ORDER BY ' + Column.FieldName + ';';
    Recorded_ADOQuery.SQL.Add(sSQL);
    Recorded_ADOQuery.ExecSQL;
    Recorded_ADOQuery.Active := true;
  except
    on E: Exception do m_Trace.DBMSG(TRACE_ERROR,'Recorded_DBGridTitleClick '+ E.Message );
  end;
end;

////////////////////////////////////////////////////////////////////////////////
//
// Kanalliste
//
procedure TfrmMain.FillDbChannelListBox(iBouquetId: Integer);
var
  sTmp,
  sName,
  sSQL     : String;
begin
  m_Trace.DBMSG(TRACE_CALLSTACK, '> FillDbChannelListBox');
  try
    lbxChannelList.Items.BeginUpdate;
    lbxChannelList.Items.Clear;
    try
      Work_ADOQuery.SQL.Clear;
      if iBouquetId < 1 then
      begin
        sSQL := 'SELECT * FROM T_Channel WHERE BouquetId < 99 ORDER BY BouquetId, Name;';
      end else
      begin
        sSQL := 'SELECT * FROM T_Channel WHERE BouquetId = '+IntToStr(iBouquetId)+' ORDER BY Name;';
      end;
      Work_ADOQuery.SQL.Add(sSQL);
      Work_ADOQuery.Active := true;
      if not Work_ADOQuery.Recordset.Eof then
      begin
        repeat
          sTmp := VarToStrDef(Work_ADOQuery.Recordset.Fields['ChannelId'].Value, '0');
          if sTmp <> '0' then
          begin
            sName := VarToStrDef(Work_ADOQuery.Recordset.Fields['Name'].Value, '?');
            lbxChannelList.Items.Add(sName);
          end;
          Work_ADOQuery.Recordset.MoveNext;
        until(Work_ADOQuery.Recordset.Eof);
        lbxChannelList.ItemIndex := 0;
      end;
    finally
      lbxChannelList.Items.EndUpdate;
    end;
    DoEvents;
    lbxChannelListClick(self);
  except
    on E: Exception do m_Trace.DBMSG(TRACE_ERROR, 'FillDbChannelListBox '+E.Message +' SQL:'+ sSQL);
  end;
  m_Trace.DBMSG(TRACE_CALLSTACK, '< FillDbChannelListBox');
end;


////////////////////////////////////////////////////////////////////////////////
//
// EventListe
//
procedure TfrmMain.FillDbEventListView( ChannelId : String );
var
  i        : Integer;
  sTmp,
  sSQL,
  sZielformat,
  sStatus,
  sColor   : String;
  sEventId : String;
  sZeit    : String;
  sDatum   : String;
  sUhrzeit : String;
  sDauer   : String;
  sTitel   : String;
  sSubTitel: String;
  sEpg     : String;
  Datum    : TDateTime;
  ListItem : TListItem;
begin
  m_Trace.DBMSG(TRACE_CALLSTACK, '> FillDbEventListView ' + ChannelId);
  try
    lvChannelProg.Items.Clear;
    lvChannelProg.Items.BeginUpdate;
    try
      sZeit := IntToStr(DateTimeToUnix(IncHour((Now - OffsetFromUTC),-3)));
      Work_ADOQuery.SQL.Clear;

      sSQL :=        'SELECT ChannelId, EventId, Zeit, Dauer, Titel, SubTitel, EPG, TimerZielformat, TimerStatus ';
      sSQL := sSQL + 'FROM v_Events ';
      sSQL := sSQL + 'WHERE ';
      sSQL := sSQL + 'ChannelId = ''' + ChannelId + ''' AND ';
      sSQL := sSQL + 'Zeit     >= ''' + CheckDbString(sZeit) + ''' ';
      sSQL := sSQL + 'ORDER BY Zeit ;';
      Work_ADOQuery.SQL.Add(sSQL);
      Work_ADOQuery.Active := true;
      if not Work_ADOQuery.Recordset.Eof then
      begin
        repeat
          sTmp := VarToStrDef(Work_ADOQuery.Recordset.Fields['ChannelId'].Value, '0');
          if sTmp <> '0' then
          begin
            try            //dezimal-string ...
              sEventId  := VarToStrDef(Work_ADOQuery.Recordset.Fields['EventId'].Value, '0');
              if Length(sEventId) = 10 then //hex ???
                sEventId  := IntToStr(StrToInt64Ex(sEventId));
            except
              on E: Exception do m_Trace.DBMSG(TRACE_ERROR, 'Error sEventId '+E.Message);
            end;
            try
              sZielformat:= VarToStrDef(Work_ADOQuery.Recordset.Fields['TimerZielformat'].Value, '-1');
              sStatus    := VarToStrDef(Work_ADOQuery.Recordset.Fields['TimerStatus'].Value, '0');
            except
              on E: Exception do m_Trace.DBMSG(TRACE_ERROR, 'Error sZeit '+E.Message);
            end;
            try
              sZeit     := VarToStrDef(Work_ADOQuery.Recordset.Fields['Zeit'].Value, '0');
            except
              on E: Exception do m_Trace.DBMSG(TRACE_ERROR, 'Error sZeit '+E.Message);
            end;
            try            //from sec to min ...
              sDauer    := IntToStr(StrToInt(VarToStrDef(Work_ADOQuery.Recordset.Fields['Dauer'].Value, '0')) div 60);
            except
              on E: Exception do m_Trace.DBMSG(TRACE_ERROR, 'Error sDauer '+E.Message);
            end;
            try
              sTitel    := VarToStrDef(Work_ADOQuery.Recordset.Fields['Titel'].Value, '?');
            except
              on E: Exception do m_Trace.DBMSG(TRACE_ERROR, 'Error sTitel '+E.Message);
            end;
            try
              sSubTitel := VarToStrDef(Work_ADOQuery.Recordset.Fields['SubTitel'].Value, '');
            except
              on E: Exception do m_Trace.DBMSG(TRACE_ERROR, 'Error sSubTitel '+E.Message);
            end;
            try
              sEpg      := VarToStrDef(Work_ADOQuery.Recordset.Fields['Epg'].Value, '');
            except
              on E: Exception do m_Trace.DBMSG(TRACE_ERROR, 'Error sEpg '+E.Message);
            end;
            try            //add offset from daylightsaving ...
              Datum     := UnixToDateTime(StrToInt64Def(sZeit,0)) + OffsetFromUTC;
              DateTimeToString(sDatum,   'dd.mm.yyyy', Datum);
              DateTimeToString(sUhrzeit, 'hh:nn', Datum);
            except
              on E: Exception do m_Trace.DBMSG(TRACE_ERROR, 'Error Datum '+E.Message);
            end;
            try
              ListItem := lvChannelProg.Items.Add;
              ListItem.Caption := sEventId; //unvisible ...
              ListItem.SubItems.Add(sZeit);
              ListItem.SubItems.Add(sDatum);
              ListItem.SubItems.Add(sUhrzeit);
              ListItem.SubItems.Add(sDauer);
              if sSubTitel <> '' then
              begin
                ListItem.SubItems.Add(sTitel + ' - ' + sSubTitel);
              end else
              begin
                ListItem.SubItems.Add(sTitel);
              end;
              ListItem.SubItems.Add(sEpg); // = Item.SubItems[5]

              if sStatus = '1' then
              begin
                ListItem.SubItems.Add(sZielformat); // = Item.SubItems[6]
              end else
              begin
                ListItem.SubItems.Add('-1'); // = Item.SubItems[6]
//                sColor := IntToStr(m_coVcrDb.GetTimerTargetType(ChannelId, sEventId));
              end;
            except
              on E: Exception do m_Trace.DBMSG(TRACE_ERROR, 'Error lvChannelProg.Items... '+E.Message);
            end;
          end;
          Work_ADOQuery.Recordset.MoveNext;
        until(Work_ADOQuery.Recordset.Eof);
        GetCurrentEvent(ChannelId);
        DoEvents;
      end;
    finally
      lvChannelProg.Items.EndUpdate;
    end;
  except
    on E: Exception do m_Trace.DBMSG(TRACE_ERROR, 'FillDbChannelListBox '+E.Message +' SQL:'+ sSQL);
  end;
  m_Trace.DBMSG(TRACE_CALLSTACK, '< FillDbEventListView');
end;

//------------------------------------------------------------------------------
//
function TfrmMain.GetCurrentDbEvent(ChannelId : String): String;
var
  sEventId,
  sSQL      : String;
begin
  Result := '';
  m_Trace.DBMSG(TRACE_CALLSTACK, '> GetCurrentDbEvent');
  try
    sEventId  := m_coVcrDb.GetCurrentEventId(ChannelId);
    if sEventId <> '0' then
    begin
      lvChannelProg.Selected:= lvChannelProg.FindCaption(1,sEventId,false,true,true);
      Result := sEventId;
      try
        if lvChannelProg.Visible AND (pclMain.ActivePage = tbsWelcome) then lvChannelProg.SetFocus;
      except
      end;
      DoEvents;
    end;
  except
    on E: Exception do m_Trace.DBMSG(TRACE_ERROR, 'GetCurrentDbEvent '+E.Message +' SQL:'+ sSQL);
  end;
  m_Trace.DBMSG(TRACE_CALLSTACK, '< GetCurrentDbEvent');
end;

//------------------------------------------------------------------------------
//
procedure TfrmMain.CheckChannelsInDb;
var
  EpgId        : Integer;
  sDbg         : String;
  sTmp,
  sSQL         : String;
  sChannelName : String;
  sChannelId   : String;
  sEventId : String;
  sZeit    : String;
  sEndZeit : String;
  sDatum   : String;
  sUhrzeit : String;
  sDauer   : String;
  sTitel   : String;
  sSubTitel: String;
  sEPG     : String;
  sTimerStr: String;
  Datum    : TDateTime;
begin
  try
    m_Trace.DBMSG(TRACE_CALLSTACK, '> CheckChannelsInDb');

    sZeit := IntToStr( DateTimeToUnix(Now() - OffsetFromUTC() ) );
    Work_ADOQuery.SQL.Clear;
    sSQL :=        'SELECT ';
    sSQL := sSQL + 'ChannelId, EventId, Name, ';
    sSQL := sSQL + 'Titel, SubTitel, Zeit, Dauer, Epg ';
    sSQL := sSQL + 'FROM ';
    sSQL := sSQL + 'v_Seek ';
    sSQL := sSQL + 'WHERE ';
    sSQL := sSQL + 'Zeit > ''' + CheckDbString(sZeit) + ''' ';
    sSQL := sSQL + 'ORDER BY Zeit ASC ;';
    Work_ADOQuery.SQL.Add(sSQL);
    Work_ADOQuery.Active := true;
    if not Work_ADOQuery.Recordset.Eof then
    begin
      Work_ADOQuery.Recordset.MoveLast;
      Work_ADOQuery.Recordset.MoveFirst;
      Whishes_Result.Items.Clear;
      Whishes_Result.Items.Add('Durchsuche die EPG-Datenbank nach vorgemerkten Sendungen ...');
      DoEvents;
      repeat
        sChannelId   := VarToStrDef(Work_ADOQuery.Recordset.Fields['ChannelId'].Value, '');
        sTitel       := VarToStrDef(Work_ADOQuery.Recordset.Fields['Titel'].Value, '');
        sSubTitel    := VarToStrDef(Work_ADOQuery.Recordset.Fields['SubTitel'].Value, '');
        sEventId     := VarToStrDef(Work_ADOQuery.Recordset.Fields['EventId'].Value, '');
        sZeit        := VarToStrDef(Work_ADOQuery.Recordset.Fields['Zeit'].Value, '');
        sDauer       := VarToStrDef(Work_ADOQuery.Recordset.Fields['Dauer'].Value, '');
        sChannelName := VarToStrDef(Work_ADOQuery.Recordset.Fields['Name'].Value, '');
        sEPG         := VarToStrDef(Work_ADOQuery.Recordset.Fields['Epg'].Value, '');
        if Length(sSubTitel) > 0 then
          sTmp := sTitel + ' - ' + sSubTitel
        else
          sTmp := sTitel;
          
        try
          Datum  := UnixToDateTime(StrToInt64Def(sZeit,0)) + OffsetFromUTC;
        except
        end;
        DateTimeToString(sDatum, 'dd.mm.yyyy', Datum);
        DateTimeToString(sUhrzeit, 'hh:nn', Datum);

        lbl_check_EPG.Caption:= sDatum +' '+ sUhrzeit +' "'+ sChannelName +'"';
        DoEvents;

        sDbg := 'CheckChannelProg '+sChannelName+' eventid=' + sEventId + ' Titel="' + sTmp  + '"';
        m_Trace.DBMSG(TRACE_SYNC, sDbg);
        DoEvents;
        
        if m_coVcrDb.IsInWhishesList(sTitel, sSubTitel) > 0 then
        begin
          sDbg:= sDatum+' '+sUhrzeit+' "'+sTmp+'" auf "'+sChannelName+'" gefunden';
          m_Trace.DBMSG(TRACE_DETAIL, sDbg);

          sEndZeit  := IntToStr( StrToIntDef(sZeit,0) + StrToIntDef(sDauer,0) );
          EpgId:= m_coVcrDb.InsertEpg(sTitel, sSubTitel, sEpg);
          m_coVcrDb.UpdateTimerInDb( sChannelId,
                                     sEventId,
                                     sZeit,
                                     sEndZeit,
                                     sTitel,
                                     EpgId,
                                     0 ); //Timer InAktiv setzen...


          sTimerStr :=             sChannelId + '|||';
          sTimerStr := sTimerStr + sEventId;

          Whishes_Result.Items.Add(sDbg + '                                                                                                                                                                                   ### ' + sTimerStr);
        end;
        Work_ADOQuery.Recordset.MoveNext;
      until(Work_ADOQuery.Recordset.Eof);

      Whishes_Result.Items.Add('... fertig');
    end;

  except
    on E: Exception do m_Trace.DBMSG(TRACE_ERROR, 'CheckChannelsInDb '+E.Message);
  end;
  m_Trace.DBMSG(TRACE_CALLSTACK, '< CheckChannelsInDb');
end;

//------------------------------------------------------------------------------
//
procedure TfrmMain.CheckChannelProgInDb(ChannelId: String);
var
  sDbg     : String;
  sTmp     : String;
  sLabel   : String;
  iLabel   : Integer;
  sLine    : String;
  sEventId : String;
  sFstEvId : String;
  sZeit    : String;
  sDatum   : String;
  sUhrzeit : String;
  sDauer   : String;
  sTitel   : String;
  sSubTitel: String;
  sEpg     : String;
  sTimerStr: String;
  Datum    : TDateTime;
begin
  m_Trace.DBMSG(TRACE_CALLSTACK, '> CheckChannelProg');
  try
    if not PingDBox(m_sDBoxIp) then
    begin
      m_Trace.DBMSG(TRACE_CALLSTACK, '< CheckChannelProg (no PING)');
      exit;
    end;

    if IsDBoxRecording then
    begin
      m_Trace.DBMSG(TRACE_CALLSTACK, '< CheckChannelProg (DBox is recording now)');
      exit;
    end;

    sDbg := 'CheckChannelProg /control/epg?' + (ChannelId);
    m_Trace.DBMSG(TRACE_SYNC, sDbg);

    DateSeparator   := '-';
    ShortDateFormat := 'yyyy/m/d';
    TimeSeparator   := '.';

    sLabel := lbl_check_EPG.Caption;
    iLabel := 1;

    sTmp := SendHttpCommand('/control/epg?' + (ChannelId));
    if Length(sTmp) > 0 then
    begin
      sTmp := sTmp + #10;
      repeat
        sLine := Copy(sTmp, 1, Pos(#10,sTmp)-1);
        if sLine = '' then
        begin
          m_Trace.DBMSG(TRACE_CALLSTACK, '< CheckChannelProg (line is empty)');
          exit;
        end;
        m_Trace.DBMSG(TRACE_SYNC, sLine);

        case iLabel of
          1: begin
               iLabel:= 2;
               lbl_check_EPG.Caption:= sLabel + ' .';
             end;
          2: begin
               iLabel:= 3;
               lbl_check_EPG.Caption:= sLabel + ' ..';
             end;
          3: begin
               iLabel:= 1;
               lbl_check_EPG.Caption:= sLabel + ' ...';
             end;
        end;
        DoEvents;

        sTmp := Copy(sTmp, Pos(#10,sTmp)+1, Length(sTmp));
        ////////
        try
          sEventId := Copy(sLine, 1, Pos(' ',sLine)-1);
        except
        end;
        if sFstEvId = '' then
        begin
          sFstEvId := sEventId;
        end;
        sLine    := Copy(sLine, Pos(' ',sLine)+1, Length(sLine));
        sZeit    := Copy(sLine, 1, Pos(' ',sLine)-1);
        ////////
        Datum := Now();
        try
          Datum  := UnixToDateTime(StrToInt64Def(sZeit,0)) + OffsetFromUTC;
        except
        end;
        if Datum > Now() then
        begin
          DateTimeToString(sDatum, 'dd.mm.yyyy', Datum);
          DateTimeToString(sUhrzeit, 'hh:nn', Datum);
          ////////
          sLine    := Copy(sLine, Pos(' ',sLine)+1, Length(sLine));
          sDauer   := IntToStr(StrToIntDef(Trim(Copy(sLine, 1, Pos(' ',sLine)-1)),60));
          m_Trace.DBMSG(TRACE_SYNC, 'Dauer '+sDauer);
          ////////
          sTitel   := Copy(sLine, Pos(' ',sLine)+1, Length(sLine));
          sEpg     := SaveEpgText(IntToHex(StrToInt64Ex(sEventId),10), '', sTitel, sSubTitel).Text;
          ////////

{
          UpdateEventInDb(ChannelId,
                          sEventId,
                          sZeit,
                          sDauer,
                          sTitel,
                          sSubTitel,
                          sEpg);
}
          DoEvents;

          sDbg := 'CheckChannelProg eventid=' + sEventId + ' Titel="' + sTitel + '" SubTilel="' + sSubTitel  + '"';
          m_Trace.DBMSG(TRACE_SYNC, sDbg);
          DoEvents;
          if m_coVcrDb.IsInWhishesList(sTitel) > 0 then
          begin
            sDbg:= 'Titel "'+sTitel+'" auf "'+m_coVcrDb.GetDbChannelName(ChannelId)+'" am '+sDatum+' um '+sUhrzeit+' gefunden !';
            m_Trace.DBMSG(TRACE_DETAIL, sDbg);

            sTimerStr := '/fb/timer.dbox2?action=new&type=5&alarm=' + sZeit;
            sTimerStr := sTimerStr + '&stop=' + IntToStr( StrToInt64Def(sZeit,0) + StrToIntDef(sDauer,0) );
            sTimerStr := sTimerStr + '&channel_id=' + ChannelId + '&rs=1';

            Whishes_Result.Items.Add(sDbg + '                                                                                                    ### ' + sTimerStr);
          end;
        end;
        if IsDBoxRecording then
        begin
          m_Trace.DBMSG(TRACE_CALLSTACK, '< CheckChannelProg (DBox is recording now)');
          exit;
        end;
        DoEvents;
      until( Length(sTmp) <= 0 );
      DoEvents;
    end;
  except
    on E: Exception do m_Trace.DBMSG(TRACE_ERROR, 'CheckChannelProg '+E.Message);
  end;
  m_Trace.DBMSG(TRACE_CALLSTACK, '< CheckChannelProg');
end;

//
//  DataBase - Procedures ...
//
////////////////////////////////////////////////////////////////////////////////
