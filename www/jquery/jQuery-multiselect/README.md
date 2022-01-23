jQuery MultiSelect
==================

Turn a multiselect list into a nice and easy to use list with checkboxes.  This plugin is simply an alternative interface for the native select list element.  When you check an option in the plugin the value is selected on the native list.  This allows the value to be submitted in a form as well as retreived through normal POST/GET and javascript methods.

## Support
I only provide limited support through the Github Issues area.  DO NOT ask for support via email, socialmedia, or other means.  Also check the closed issues before opening a new issue.

## Design Methodology
This plugin is not meant to be a full featured multiselect plugin.  I envision this as a multiselect plugin that contains a base set of features that can be enhanced using callbacks by the developer.  Because of this I am hesitant on adding functionality but am open to adding callbacks where it makes sense.  But feel free to open an [issue](https://github.com/nobleclem/jQuery-MultiSelect/issues) and suggest a feature request if you feel that most people will benefit from it.

## Demo
http://springstubbe.us/projects/jquery-multiselect/

## Usage
```
// BASIC
$('select[multiple]').multiselect();

// 4 COLUMNS with custom placeholder text
$('select[multiple]').multiselect({
    columns: 4,
    texts: {
        placeholder: 'Select options'
    }
});

// RELOAD multiselect (in case you modify options or selected options in the native select list since loading the plugin)
$('select[multiple]').multiselect('reload');

// DYNAMICALLY LOAD OPTIONS
$('select[multiple]').multiselect( 'loadOptions', [{
    name   : 'Option Name 1',
    value  : 'option-value-1',
    checked: false,
    attributes : {
        custom1: 'value1',
        custom2: 'value2'
    }
},{
    name   : 'Option Name 2',
    value  : 'option-value-2',
    checked: false,
    attributes : {
        custom1: 'value1',
        custom2: 'value2'
    }
}]);
```


### Options
| Option              | Values   | Default        | Description                    |
| ------------------- | -------- | -------------- | ------------------------------ |
| columns             | int      | 1              | # of columns to show options   |
| search              | bool     | false          | enable option search/filering  |
| searchOptions       | object   |                |                                |
| - delay             | int      | 250            | time (in ms) between keystrokes until search happens |
| - showOptGroups     | bool     | false          | show option group titles if no options remaining |
| - searchText        | bool     | true           | search within option text      |
| - searchValue       | bool     | false          | search within option value     |
| - onSearch          | function |                | fires before search on options happens |
| texts               | object   |                |                                |
| - placeholder       | string   | Select options | default text for dropdown      |
| - search            | string   | Search         | search input placeholder text  |
| - selectedOptions   | string   |  selected      | selected suffix text           |
| - selectAll         | string   | Select all     | select all text                |
| - unselectAll       | string   | Unselect all   | unselect all text              |
| - noneSelected      | string   | None Selected  | None selected text             |
| selectAll           | bool     | false          | add select all option          |
| selectGroup         | bool     | false          | add select all optgroup option |
| minHeight           | number   | 200            | min height of option overlay   |
| maxHeight           | number   | null           | max height of option overlay   |
| maxWidth            | number   | null           | maximum width of option overlay (or selector) |
| maxPlaceholderWidth | number   | null           | maximum width of placeholder button |
| maxPlaceholderOpts  | number   | 10             | maximum number of placeholder options to show until "# selected" shown instead |
| showCheckbox        | bool     | true           | display the option checkbox    |
| checkboxAutoFit     | bool     | false          | auto calc space requirements for checkbox instead of css padding on label |
| onLoad              | function |                | fires at end of initial loading, hides native select list |
| onOptionClick       | function |                | fires after on option is clicked |
| onControlClose      | function |                | fires when the options list is closed |
| onSelectAll         | function |                | fires when (un)select all is clicked |
| onPlaceholder       | function |                | fires when the placeholder txt is updated |
| optionAttributes    | array    |                | array of attribute keys to copy to the checkbox input |


### Methods
**loadOptions( options, overwrite, updateSelect )**

Update options of select list. Default state will replace existing list with this one.
- *Set the second parameter to `false` to append to the list. (default = true)*
- *Set the third parameter to `false` to leave the native select list as is. (default = true)*

*This will NOT modify the original select list element.*
```
$('select[multiple]').multiselect( 'loadOptions', [{
    name   : 'Option Name 1',
    value  : 'option-value-1',
    checked: false
},{
    name   : 'Option Name 2',
    value  : 'option-value-2',
    checked: false
}]);
```


**settings**

Update Multiselect list settings after it has been rendered.  It accepts the same [options](https://github.com/nobleclem/jQuery-MultiSelect#options) listed above.

*This will reload the plugin for the select list it references*

`$('select[multiple]').multiselect( 'settings', {
    columns: 2
});`

**unload**

Disable the jquery multiselect list and show the native select list.

*This is distructive. You will have to reinitialize with all options to enable the plugin for the list.*

`$('select[multiple]').multiselect( 'unload' );`


**reload**

This is a quick unload/load while maintaining options during plugin initialization.

`$('select[multiple]').multiselect( 'reload' );`


**reset**

Reset the element back to its default selected values.

`$('select[multiple]').multiselect( 'reset' );`


**disable**

Disable or enable the select list. If no second parameter is passed then true is assumed.

`$('select[multiple]').multiselect( 'disable', true );`
`$('select[multiple]').multiselect( 'disable', false );`


### Callbacks
**onLoad**

Fires after initial loading and hides native select list

`onLoad( element )`

element: select list element object


**onOptionClick**

*Fires after an option is clicked*

`onOptionClick( element, option )`

element: select list element object

option:  option element object


**onControlClose**

Fires when the options list is closed

`onControlClose( element )`

element: select list element object


**onSelectAll**

Fires when (un)select all is clicked

`onSelectAll( element, selected )`

element: select list element object

selected: the total number of options selected

**onPlaceholder**

Fires when the placeholder txt is updated (only fires if there are selected options)

`onPlaceholder( element, placeholder, selectedOpts )`

element: select list element object

placeholder: placeholder element object

selectedOpts: selected options


**onSearch**

*Fires before search on options happens*

`searchOptions.onSearch( element )`

element: select list element object
