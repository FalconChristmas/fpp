$(function(){
    $("#myDummyTable").tablesorter({widgets: ['zebra']});
    $.extend($.tablesorter.themes.bootstrap, {
      // table classes
      table: 'table table-bordered table-striped',
      caption: 'caption',
      // *** header class names ***
      // header classes
      header: 'bootstrap-header',
      sortNone: '',
      sortAsc: '',
      sortDesc: '',
      // applied when column is sorted
      active: '',
      // hover class
      hover: '',
      // *** icon class names ***
      // icon class added to the <i> in the header
      icons: '',
      // class name added to icon when column is not sorted
      iconSortNone: 'bootstrap-icon-unsorted',
      // class name added to icon when column has ascending sort
      iconSortAsc: 'icon-chevron-up glyphicon glyphicon-chevron-up sort-asc',
      // class name added to icon when column has descending sort
      iconSortDesc: 'icon-chevron-down glyphicon glyphicon-chevron-down',
      filterRow: '',
      footerRow: '',
      footerCells: '',
      // even row zebra striping
      even: '',
      // odd row zebra striping
      odd: ''
  });
  });